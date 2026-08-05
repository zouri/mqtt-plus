#include "luaruntimeadapter.h"

#include <QCborArray>
#include <QCborMap>
#include <QDeadlineTimer>
#include <QRegularExpression>
#include <QSet>
#include <QStringDecoder>

#include <algorithm>
#include <cmath>
#include <cstring>
#include <limits>
#include <memory>
#include <utility>

extern "C" {
#include <lauxlib.h>
#include <lua.h>
#include <lualib.h>
}

namespace {

constexpr int kHookInstructionInterval = 1000;
constexpr int kMaximumInstructions = 200000;
constexpr int kStackReserve = 12;
int kNullValueMarker;
int kByteValueMarkerKey;

struct LuaByteValueHeader
{
    size_t size = 0;
};

struct LuaStateDeleter
{
    void operator()(lua_State *state) const
    {
        if (state) {
            lua_close(state);
        }
    }
};

using LuaState = std::unique_ptr<lua_State, LuaStateDeleter>;

struct LuaChunk
{
    QString path;
    QByteArray bytecode;
};

class LuaPreparedProcessor final : public PreparedProcessor
{
public:
    QVector<LuaChunk> chunks;
    QString entrySymbol;
};

struct LuaExecutionBudget
{
    QDeadlineTimer deadline;
    int executedInstructions = 0;
    bool exceeded = false;
};

static_assert(LUA_EXTRASPACE >= sizeof(LuaExecutionBudget *));

struct LuaCallFailure
{
    QString message;
    QString file;
    int line = -1;
    bool timedOut = false;
};

enum class LuaValueIssue
{
    None,
    Unsupported,
    LimitExceeded,
};

struct LuaValueResult
{
    LuaValueIssue issue = LuaValueIssue::None;
    QCborValue value;
    QString message;
};

ProcessorDiagnostic diagnostic(
    const QString &code,
    const QString &message,
    const QString &file = {},
    int line = -1)
{
    ProcessorDiagnostic result;
    result.code = code;
    result.message = message;
    result.file = file;
    result.line = line;
    return result;
}

QString luaString(lua_State *state, int index)
{
    size_t length = 0;
    const char *data = lua_tolstring(state, index, &length);
    return data ? QString::fromUtf8(data, qsizetype(length)) : QString();
}

QByteArray luaBytes(lua_State *state, int index)
{
    size_t length = 0;
    const char *data = lua_tolstring(state, index, &length);
    return data
        ? QByteArray(data, static_cast<qsizetype>(length))
        : QByteArray();
}

void pushBytes(lua_State *state, const QByteArray &value)
{
    lua_pushlstring(
        state,
        value.constData(),
        static_cast<size_t>(value.size()));
}

void pushString(lua_State *state, const QString &value)
{
    pushBytes(state, value.toUtf8());
}

void clearGlobal(lua_State *state, const char *name)
{
    lua_pushnil(state);
    lua_setglobal(state, name);
}

int createByteValue(lua_State *state)
{
    size_t size = 0;
    const char *data = luaL_checklstring(state, 1, &size);
    if (size > (std::numeric_limits<size_t>::max)() - sizeof(LuaByteValueHeader)) {
        return luaL_error(state, "Lua byte value is too large.");
    }
    auto *header = static_cast<LuaByteValueHeader *>(
        lua_newuserdatauv(state, sizeof(LuaByteValueHeader) + size, 0));
    header->size = size;
    if (size > 0) {
        std::memcpy(header + 1, data, size);
    }
    luaL_getmetatable(state, "mqtt-plus.byte-value");
    lua_setmetatable(state, -2);
    return 1;
}

int byteValueLength(lua_State *state)
{
    const auto *header = static_cast<const LuaByteValueHeader *>(
        lua_touserdata(state, 1));
    lua_pushinteger(
        state,
        header ? static_cast<lua_Integer>(header->size) : 0);
    return 1;
}

int byteValueToString(lua_State *state)
{
    const auto *header = static_cast<const LuaByteValueHeader *>(
        lua_touserdata(state, 1));
    if (!header) {
        lua_pushliteral(state, "");
        return 1;
    }
    lua_pushlstring(
        state,
        reinterpret_cast<const char *>(header + 1),
        header->size);
    return 1;
}

bool readByteValue(lua_State *state, int index, QByteArray &result)
{
    const int absoluteIndex = lua_absindex(state, index);
    if (lua_type(state, absoluteIndex) != LUA_TUSERDATA
        || lua_getmetatable(state, absoluteIndex) == 0) {
        return false;
    }
    lua_pushlightuserdata(state, &kByteValueMarkerKey);
    lua_rawget(state, -2);
    const bool isByteValue = lua_toboolean(state, -1) != 0;
    lua_pop(state, 2);
    if (!isByteValue) {
        return false;
    }

    const auto *header = static_cast<const LuaByteValueHeader *>(
        lua_touserdata(state, absoluteIndex));
    const size_t allocationSize = lua_rawlen(state, absoluteIndex);
    if (!header
        || allocationSize < sizeof(LuaByteValueHeader)
        || header->size > allocationSize - sizeof(LuaByteValueHeader)
        || header->size > static_cast<size_t>((std::numeric_limits<qsizetype>::max)())) {
        return false;
    }
    result = QByteArray(
        reinterpret_cast<const char *>(header + 1),
        static_cast<qsizetype>(header->size));
    return true;
}

void openSafeLibraries(lua_State *state)
{
    luaL_requiref(state, "_G", luaopen_base, 1);
    lua_pop(state, 1);
    luaL_requiref(state, LUA_STRLIBNAME, luaopen_string, 1);
    lua_pop(state, 1);
    luaL_requiref(state, LUA_MATHLIBNAME, luaopen_math, 1);
    lua_pop(state, 1);
    luaL_requiref(state, LUA_TABLIBNAME, luaopen_table, 1);
    lua_pop(state, 1);
    luaL_requiref(state, LUA_UTF8LIBNAME, luaopen_utf8, 1);
    lua_pop(state, 1);

    clearGlobal(state, "collectgarbage");
    clearGlobal(state, "dofile");
    clearGlobal(state, "load");
    clearGlobal(state, "loadfile");
    clearGlobal(state, "print");
    clearGlobal(state, "rawset");

    if (luaL_newmetatable(state, "mqtt-plus.byte-value") != 0) {
        lua_pushlightuserdata(state, &kByteValueMarkerKey);
        lua_pushboolean(state, true);
        lua_rawset(state, -3);
        lua_pushcfunction(state, byteValueLength);
        lua_setfield(state, -2, "__len");
        lua_pushcfunction(state, byteValueToString);
        lua_setfield(state, -2, "__tostring");
        lua_pushliteral(state, "protected");
        lua_setfield(state, -2, "__metatable");
    }
    lua_pop(state, 1);
    lua_pushcfunction(state, createByteValue);
    lua_setglobal(state, "bytes");

    lua_pushlightuserdata(state, &kNullValueMarker);
    lua_setglobal(state, "null");

    lua_pushliteral(state, "");
    if (lua_getmetatable(state, -1) != 0) {
        lua_pushliteral(state, "protected");
        lua_setfield(state, -2, "__metatable");
        lua_pop(state, 1);
    }
    lua_pop(state, 1);
}

LuaState createState()
{
    LuaState state(luaL_newstate());
    if (state) {
        openSafeLibraries(state.get());
    }
    return state;
}

void instructionHook(lua_State *state, lua_Debug *)
{
    auto **budgetPointer = static_cast<LuaExecutionBudget **>(lua_getextraspace(state));
    LuaExecutionBudget *budget = budgetPointer ? *budgetPointer : nullptr;
    if (!budget) {
        return;
    }

    budget->executedInstructions += kHookInstructionInterval;
    if (budget->deadline.hasExpired()
        || budget->executedInstructions >= kMaximumInstructions) {
        budget->exceeded = true;
        luaL_error(state, "Lua execution limit exceeded.");
    }
}

void installBudget(
    lua_State *state,
    LuaExecutionBudget &budget,
    const ProcessorExecutionLimits &limits)
{
    budget.deadline.setRemainingTime(
        limits.wallTimeMilliseconds,
        Qt::PreciseTimer);
    auto **budgetPointer = static_cast<LuaExecutionBudget **>(lua_getextraspace(state));
    *budgetPointer = &budget;
    lua_sethook(state, instructionHook, LUA_MASKCOUNT, kHookInstructionInterval);
}

LuaCallFailure callFailure(
    lua_State *state,
    const LuaExecutionBudget &budget)
{
    LuaCallFailure result;
    result.message = luaString(state, -1);
    result.timedOut = budget.exceeded || budget.deadline.hasExpired();

    static const QRegularExpression locationExpression(
        QStringLiteral("^(.+):(\\d+):\\s*(.*)$"));
    const QRegularExpressionMatch match = locationExpression.match(result.message);
    if (match.hasMatch()) {
        result.file = match.captured(1);
        result.line = match.captured(2).toInt();
        result.message = match.captured(3);
    }
    return result;
}

int dumpBytecode(lua_State *, const void *data, size_t size, void *userData)
{
    auto *bytecode = static_cast<QByteArray *>(userData);
    bytecode->append(static_cast<const char *>(data), static_cast<qsizetype>(size));
    return 0;
}

bool setChunkEnvironment(lua_State *state, int chunkIndex, int environmentIndex)
{
    const int absoluteChunkIndex = lua_absindex(state, chunkIndex);
    lua_pushvalue(state, environmentIndex);
    return lua_setupvalue(state, absoluteChunkIndex, 1) != nullptr;
}

bool isLuaSourceFile(const ProcessorSourceFile &file)
{
    return file.path.endsWith(QStringLiteral(".lua"), Qt::CaseInsensitive)
        || file.mediaType.contains(QStringLiteral("lua"), Qt::CaseInsensitive);
}

QVector<const ProcessorSourceFile *> orderedLuaFiles(
    const ProcessorRevisionSnapshot &revision)
{
    QVector<const ProcessorSourceFile *> helperFiles;
    const ProcessorSourceFile *entryFile = nullptr;
    for (const ProcessorSourceFile &file : revision.files) {
        if (file.path == revision.entryFile) {
            entryFile = &file;
        } else if (isLuaSourceFile(file)) {
            helperFiles.append(&file);
        }
    }
    std::sort(
        helperFiles.begin(),
        helperFiles.end(),
        [](const ProcessorSourceFile *left, const ProcessorSourceFile *right) {
            return left->path < right->path;
        });
    if (entryFile && isLuaSourceFile(*entryFile)) {
        helperFiles.append(entryFile);
    }
    return helperFiles;
}

bool compileChunk(
    lua_State *state,
    const ProcessorSourceFile &file,
    LuaChunk &chunk,
    LuaCallFailure &failure)
{
    const QByteArray chunkName = QByteArrayLiteral("@") + file.path.toUtf8();
    if (luaL_loadbuffer(
            state,
            file.content.constData(),
            static_cast<size_t>(file.content.size()),
            chunkName.constData()) != LUA_OK) {
        LuaExecutionBudget budget;
        failure = callFailure(state, budget);
        return false;
    }
    chunk.path = file.path;
    if (lua_dump(state, dumpBytecode, &chunk.bytecode, 0) != 0) {
        failure.message = QStringLiteral("Unable to cache compiled Lua bytecode.");
        lua_pop(state, 1);
        return false;
    }
    lua_pop(state, 1);
    return true;
}

bool executeChunks(
    lua_State *state,
    const QVector<LuaChunk> &chunks,
    LuaExecutionBudget &budget,
    LuaCallFailure &failure)
{
    lua_pushglobaltable(state);
    const int environmentIndex = lua_absindex(state, -1);
    for (const LuaChunk &chunk : chunks) {
        const QByteArray chunkName = QByteArrayLiteral("@") + chunk.path.toUtf8();
        if (luaL_loadbufferx(
                state,
                chunk.bytecode.constData(),
                static_cast<size_t>(chunk.bytecode.size()),
                chunkName.constData(),
                "b") != LUA_OK) {
            failure = callFailure(state, budget);
            return false;
        }
        if (!setChunkEnvironment(state, -1, environmentIndex)) {
            failure.message = QStringLiteral("Unable to isolate the Lua processor environment.");
            return false;
        }
        if (lua_pcall(state, 0, 0, 0) != LUA_OK) {
            failure = callFailure(state, budget);
            return false;
        }
        if (budget.deadline.hasExpired()) {
            budget.exceeded = true;
            failure.message = QStringLiteral("Lua execution limit exceeded.");
            failure.timedOut = true;
            return false;
        }
    }
    return true;
}

bool pushCborValue(
    lua_State *state,
    const QCborValue &value,
    int depth,
    qsizetype &entryCount,
    const ProcessorExecutionLimits &limits,
    QString &error)
{
    if (depth > limits.maxResultDepth) {
        error = QStringLiteral("Processor parameters nesting exceeds the configured limit.");
        return false;
    }
    if (lua_checkstack(state, kStackReserve) == 0) {
        error = QStringLiteral("Unable to reserve Lua stack space for processor parameters.");
        return false;
    }

    switch (value.type()) {
    case QCborValue::Null:
        lua_pushlightuserdata(state, &kNullValueMarker);
        return true;
    case QCborValue::False:
    case QCborValue::True:
        lua_pushboolean(state, value.toBool());
        return true;
    case QCborValue::Integer:
        lua_pushinteger(state, static_cast<lua_Integer>(value.toInteger()));
        return true;
    case QCborValue::Double:
        if (!std::isfinite(value.toDouble())) {
            error = QStringLiteral("Processor parameters contain a non-finite number.");
            return false;
        }
        lua_pushnumber(state, static_cast<lua_Number>(value.toDouble()));
        return true;
    case QCborValue::ByteArray:
        pushBytes(state, value.toByteArray());
        return true;
    case QCborValue::String:
        pushString(state, value.toString());
        return true;
    case QCborValue::Array: {
        const QCborArray array = value.toArray();
        if (array.size() > limits.maxCollectionEntries - entryCount) {
            error = QStringLiteral("Processor parameters contain too many collection entries.");
            return false;
        }
        entryCount += array.size();
        lua_createtable(state, static_cast<int>(array.size()), 0);
        const int tableIndex = lua_absindex(state, -1);
        for (qsizetype index = 0; index < array.size(); ++index) {
            if (!pushCborValue(
                    state,
                    array.at(index),
                    depth + 1,
                    entryCount,
                    limits,
                    error)) {
                return false;
            }
            lua_rawseti(state, tableIndex, static_cast<lua_Integer>(index + 1));
        }
        return true;
    }
    case QCborValue::Map: {
        const QCborMap map = value.toMap();
        if (map.size() > limits.maxCollectionEntries - entryCount) {
            error = QStringLiteral("Processor parameters contain too many collection entries.");
            return false;
        }
        entryCount += map.size();
        lua_createtable(state, 0, static_cast<int>(map.size()));
        const int tableIndex = lua_absindex(state, -1);
        for (auto it = map.constBegin(); it != map.constEnd(); ++it) {
            if (!it.key().isString()) {
                error = QStringLiteral("Processor parameter map keys must be strings.");
                return false;
            }
            pushString(state, it.key().toString());
            if (!pushCborValue(
                    state,
                    it.value(),
                    depth + 1,
                    entryCount,
                    limits,
                    error)) {
                return false;
            }
            lua_rawset(state, tableIndex);
        }
        return true;
    }
    default:
        error = QStringLiteral("Processor parameters contain an unsupported value type.");
        return false;
    }
}

bool pushContext(
    lua_State *state,
    const MessageProcessorContext &context,
    const ProcessorExecutionLimits &limits,
    QString &error)
{
    lua_createtable(state, 0, 7);
    const int contextIndex = lua_absindex(state, -1);

    pushString(state, context.topic);
    lua_setfield(state, contextIndex, "topic");
    pushBytes(state, context.payload);
    lua_setfield(state, contextIndex, "payload");
    pushString(state, context.receivedAt);
    lua_setfield(state, contextIndex, "receivedAt");
    pushString(state, context.format);
    lua_setfield(state, contextIndex, "format");
    pushString(state, context.decoded);
    lua_setfield(state, contextIndex, "decoded");
    pushString(state, context.decodeError);
    lua_setfield(state, contextIndex, "decodeError");

    qsizetype entryCount = 0;
    if (!pushCborValue(
            state,
            QCborValue(context.parameters),
            0,
            entryCount,
            limits,
            error)) {
        return false;
    }
    lua_setfield(state, contextIndex, "parameters");
    return true;
}

bool classifyArray(
    lua_State *state,
    int index,
    lua_Integer &arrayLength)
{
    const int absoluteIndex = lua_absindex(state, index);
    lua_Integer maximumIndex = 0;
    lua_Integer count = 0;
    lua_pushnil(state);
    while (lua_next(state, absoluteIndex) != 0) {
        if (!lua_isinteger(state, -2)) {
            lua_pop(state, 2);
            return false;
        }
        const lua_Integer key = lua_tointeger(state, -2);
        if (key < 1) {
            lua_pop(state, 2);
            return false;
        }
        maximumIndex = (std::max)(maximumIndex, key);
        ++count;
        lua_pop(state, 1);
    }
    arrayLength = maximumIndex;
    return count == maximumIndex;
}

LuaValueResult luaValueToCbor(
    lua_State *state,
    int index,
    int depth,
    qsizetype &entryCount,
    QSet<const void *> &ancestors,
    const ProcessorExecutionLimits &limits)
{
    if (depth > limits.maxResultDepth) {
        return {
            LuaValueIssue::LimitExceeded,
            {},
            QStringLiteral("Lua result nesting exceeds the configured limit."),
        };
    }
    if (lua_checkstack(state, kStackReserve) == 0) {
        return {
            LuaValueIssue::LimitExceeded,
            {},
            QStringLiteral("Unable to reserve Lua stack space for the result."),
        };
    }

    const int absoluteIndex = lua_absindex(state, index);
    switch (lua_type(state, absoluteIndex)) {
    case LUA_TNIL:
        return {LuaValueIssue::None, QCborValue(nullptr), {}};
    case LUA_TBOOLEAN:
        return {
            LuaValueIssue::None,
            QCborValue(lua_toboolean(state, absoluteIndex) != 0),
            {},
        };
    case LUA_TNUMBER:
        if (lua_isinteger(state, absoluteIndex)) {
            return {
                LuaValueIssue::None,
                QCborValue(static_cast<qint64>(lua_tointeger(state, absoluteIndex))),
                {},
            };
        }
        return {
            LuaValueIssue::None,
            QCborValue(static_cast<double>(lua_tonumber(state, absoluteIndex))),
            {},
        };
    case LUA_TSTRING: {
        const QByteArray bytes = luaBytes(state, absoluteIndex);
        QStringDecoder decoder(QStringDecoder::Utf8);
        const QString text = decoder.decode(bytes);
        return decoder.hasError() || bytes.contains('\0')
            ? LuaValueResult {LuaValueIssue::None, QCborValue(bytes), {}}
            : LuaValueResult {LuaValueIssue::None, QCborValue(text), {}};
    }
    case LUA_TLIGHTUSERDATA:
        if (lua_touserdata(state, absoluteIndex) == &kNullValueMarker) {
            return {LuaValueIssue::None, QCborValue(nullptr), {}};
        }
        return {
            LuaValueIssue::Unsupported,
            {},
            QStringLiteral("Lua returned unsupported light userdata."),
        };
    case LUA_TUSERDATA: {
        QByteArray bytes;
        if (readByteValue(state, absoluteIndex, bytes)) {
            return {LuaValueIssue::None, QCborValue(bytes), {}};
        }
        return {
            LuaValueIssue::Unsupported,
            {},
            QStringLiteral("Lua returned unsupported userdata."),
        };
    }
    case LUA_TTABLE: {
        const void *identity = lua_topointer(state, absoluteIndex);
        if (ancestors.contains(identity)) {
            return {
                LuaValueIssue::Unsupported,
                {},
                QStringLiteral("Lua result must not contain cycles."),
            };
        }
        ancestors.insert(identity);

        lua_Integer arrayLength = 0;
        if (classifyArray(state, absoluteIndex, arrayLength)) {
            if (arrayLength < 0
                || arrayLength > limits.maxCollectionEntries - entryCount) {
                ancestors.remove(identity);
                return {
                    LuaValueIssue::LimitExceeded,
                    {},
                    QStringLiteral("Lua result contains too many collection entries."),
                };
            }
            entryCount += static_cast<qsizetype>(arrayLength);
            QCborArray array;
            for (lua_Integer itemIndex = 1; itemIndex <= arrayLength; ++itemIndex) {
                lua_rawgeti(state, absoluteIndex, itemIndex);
                const LuaValueResult item = luaValueToCbor(
                    state,
                    -1,
                    depth + 1,
                    entryCount,
                    ancestors,
                    limits);
                lua_pop(state, 1);
                if (item.issue != LuaValueIssue::None) {
                    ancestors.remove(identity);
                    return item;
                }
                array.append(item.value);
            }
            ancestors.remove(identity);
            return {LuaValueIssue::None, array, {}};
        }

        QCborMap map;
        lua_pushnil(state);
        while (lua_next(state, absoluteIndex) != 0) {
            if (lua_type(state, -2) != LUA_TSTRING) {
                lua_pop(state, 2);
                ancestors.remove(identity);
                return {
                    LuaValueIssue::Unsupported,
                    {},
                    QStringLiteral("Lua result map keys must be strings."),
                };
            }
            if (entryCount >= limits.maxCollectionEntries) {
                lua_pop(state, 2);
                ancestors.remove(identity);
                return {
                    LuaValueIssue::LimitExceeded,
                    {},
                    QStringLiteral("Lua result contains too many collection entries."),
                };
            }
            ++entryCount;
            const QByteArray keyBytes = luaBytes(state, -2);
            QStringDecoder keyDecoder(QStringDecoder::Utf8);
            const QString key = keyDecoder.decode(keyBytes);
            if (keyDecoder.hasError()) {
                lua_pop(state, 2);
                ancestors.remove(identity);
                return {
                    LuaValueIssue::Unsupported,
                    {},
                    QStringLiteral("Lua result map keys must be valid UTF-8 strings."),
                };
            }
            const LuaValueResult item = luaValueToCbor(
                state,
                -1,
                depth + 1,
                entryCount,
                ancestors,
                limits);
            lua_pop(state, 1);
            if (item.issue != LuaValueIssue::None) {
                lua_pop(state, 1);
                ancestors.remove(identity);
                return item;
            }
            map.insert(key, item.value);
        }
        ancestors.remove(identity);
        return {LuaValueIssue::None, map, {}};
    }
    default:
        return {
            LuaValueIssue::Unsupported,
            {},
            QStringLiteral("Lua returned an unsupported value type."),
        };
    }
}

ProcessorPreparationResult preparationFailure(
    ProcessorValidationState state,
    const QString &code,
    const LuaCallFailure &failure)
{
    ProcessorPreparationResult result;
    result.state = state;
    result.diagnostics.append(diagnostic(
        code,
        failure.message,
        failure.file,
        failure.line));
    return result;
}

ProcessorExecutionResult executionFailure(
    ProcessorExecutionState state,
    const QString &code,
    const LuaCallFailure &failure)
{
    ProcessorExecutionResult result;
    result.state = state;
    result.diagnostics.append(diagnostic(
        code,
        failure.message,
        failure.file,
        failure.line));
    return result;
}

} // namespace

RuntimeDescriptor LuaRuntimeAdapter::descriptor() const
{
    RuntimeDescriptor result;
    result.runtimeId = QStringLiteral("lua-5.5");
    result.languageId = QStringLiteral("lua");
    result.displayName = QStringLiteral("Lua 5.5");
    result.runtimeVersion = QString::fromLatin1(LUA_RELEASE);
    result.supportedContractIds = {
        QStringLiteral("mqtt-plus.message-processor/v1"),
    };
    result.sourceExtensions = {
        QStringLiteral("lua"),
    };
    result.executionMode = RuntimeExecutionMode::ParseWorkerThread;
    return result;
}

ProcessorPreparationResult LuaRuntimeAdapter::prepare(
    const ProcessorRevisionSnapshot &revision,
    const ProcessorExecutionLimits &limits)
{
    const QVector<const ProcessorSourceFile *> sourceFiles = orderedLuaFiles(revision);
    if (sourceFiles.isEmpty()
        || sourceFiles.last()->path != revision.entryFile
        || revision.entrySymbol.contains(QChar::Null)) {
        ProcessorPreparationResult result;
        result.state = ProcessorValidationState::InvalidSource;
        result.diagnostics.append(diagnostic(
            QStringLiteral("invalid_source"),
            QStringLiteral("Lua processor entry file or entry symbol is invalid."),
            revision.entryFile));
        return result;
    }

    LuaState compiler = createState();
    if (!compiler) {
        ProcessorPreparationResult result;
        result.state = ProcessorValidationState::InternalError;
        result.diagnostics.append(diagnostic(
            QStringLiteral("internal_runtime_error"),
            QStringLiteral("Unable to create a Lua state.")));
        return result;
    }

    auto prepared = QSharedPointer<LuaPreparedProcessor>::create();
    prepared->entrySymbol = revision.entrySymbol;
    prepared->chunks.reserve(sourceFiles.size());
    for (const ProcessorSourceFile *sourceFile : sourceFiles) {
        if (sourceFile->path.contains(QChar::Null)) {
            ProcessorPreparationResult result;
            result.state = ProcessorValidationState::InvalidSource;
            result.diagnostics.append(diagnostic(
                QStringLiteral("invalid_source"),
                QStringLiteral("Lua source path contains an invalid character."),
                sourceFile->path));
            return result;
        }
        LuaChunk chunk;
        LuaCallFailure failure;
        if (!compileChunk(compiler.get(), *sourceFile, chunk, failure)) {
            return preparationFailure(
                ProcessorValidationState::InvalidSource,
                QStringLiteral("invalid_source"),
                failure);
        }
        prepared->chunks.append(std::move(chunk));
    }

    LuaExecutionBudget budget;
    LuaState validationState = createState();
    if (!validationState) {
        ProcessorPreparationResult result;
        result.state = ProcessorValidationState::InternalError;
        result.diagnostics.append(diagnostic(
            QStringLiteral("internal_runtime_error"),
            QStringLiteral("Unable to create a Lua validation state.")));
        return result;
    }
    installBudget(validationState.get(), budget, limits);
    LuaCallFailure failure;
    if (!executeChunks(validationState.get(), prepared->chunks, budget, failure)) {
        return preparationFailure(
            ProcessorValidationState::PreparationFailed,
            QStringLiteral("preparation_failed"),
            failure);
    }

    const QByteArray entrySymbol = prepared->entrySymbol.toUtf8();
    lua_getglobal(validationState.get(), entrySymbol.constData());
    if (!lua_isfunction(validationState.get(), -1)) {
        failure.message = QStringLiteral("Lua processor must define function %1(context).")
                              .arg(prepared->entrySymbol);
        failure.file = revision.entryFile;
        return preparationFailure(
            ProcessorValidationState::InvalidSource,
            QStringLiteral("invalid_source"),
            failure);
    }

    ProcessorPreparationResult result;
    result.state = ProcessorValidationState::Ready;
    result.prepared = prepared;
    return result;
}

ProcessorExecutionResult LuaRuntimeAdapter::execute(
    const PreparedProcessorHandle &prepared,
    const MessageProcessorContext &context,
    const ProcessorExecutionLimits &limits)
{
    const auto *luaPrepared = dynamic_cast<const LuaPreparedProcessor *>(prepared.data());
    if (!luaPrepared) {
        ProcessorExecutionResult result;
        result.state = ProcessorExecutionState::InternalError;
        result.diagnostics.append(diagnostic(
            QStringLiteral("internal_runtime_error"),
            QStringLiteral("Lua runtime received an incompatible prepared processor.")));
        return result;
    }

    LuaExecutionBudget budget;
    LuaState state = createState();
    if (!state) {
        ProcessorExecutionResult result;
        result.state = ProcessorExecutionState::InternalError;
        result.diagnostics.append(diagnostic(
            QStringLiteral("internal_runtime_error"),
            QStringLiteral("Unable to create a Lua execution state.")));
        return result;
    }

    installBudget(state.get(), budget, limits);
    LuaCallFailure failure;
    if (!executeChunks(state.get(), luaPrepared->chunks, budget, failure)) {
        return executionFailure(
            failure.timedOut
                ? ProcessorExecutionState::TimedOut
                : ProcessorExecutionState::ExecutionFailed,
            failure.timedOut
                ? QStringLiteral("execution_timed_out")
                : QStringLiteral("execution_failed"),
            failure);
    }

    const QByteArray entrySymbol = luaPrepared->entrySymbol.toUtf8();
    lua_getglobal(state.get(), entrySymbol.constData());
    if (!lua_isfunction(state.get(), -1)) {
        failure.message = QStringLiteral("Lua processor entry function is unavailable.");
        return executionFailure(
            ProcessorExecutionState::InternalError,
            QStringLiteral("internal_runtime_error"),
            failure);
    }

    QString contextError;
    if (!pushContext(state.get(), context, limits, contextError)) {
        failure.message = contextError;
        return executionFailure(
            ProcessorExecutionState::ExecutionFailed,
            QStringLiteral("execution_failed"),
            failure);
    }

    if (lua_pcall(state.get(), 1, 1, 0) != LUA_OK) {
        failure = callFailure(state.get(), budget);
        return executionFailure(
            failure.timedOut
                ? ProcessorExecutionState::TimedOut
                : ProcessorExecutionState::ExecutionFailed,
            failure.timedOut
                ? QStringLiteral("execution_timed_out")
                : QStringLiteral("execution_failed"),
            failure);
    }
    if (budget.deadline.hasExpired()) {
        failure.message = QStringLiteral("Lua execution limit exceeded.");
        failure.timedOut = true;
        return executionFailure(
            ProcessorExecutionState::TimedOut,
            QStringLiteral("execution_timed_out"),
            failure);
    }

    qsizetype entryCount = 0;
    QSet<const void *> ancestors;
    const LuaValueResult converted = luaValueToCbor(
        state.get(),
        -1,
        0,
        entryCount,
        ancestors,
        limits);
    if (converted.issue != LuaValueIssue::None) {
        failure.message = converted.message;
        return executionFailure(
            converted.issue == LuaValueIssue::LimitExceeded
                ? ProcessorExecutionState::OutputLimitExceeded
                : ProcessorExecutionState::UnsupportedResult,
            converted.issue == LuaValueIssue::LimitExceeded
                ? QStringLiteral("result_limit_exceeded")
                : QStringLiteral("unsupported_result"),
            failure);
    }

    ProcessorExecutionResult result;
    result.state = ProcessorExecutionState::Succeeded;
    result.value = converted.value;
    return result;
}
