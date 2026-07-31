#include "luarunner.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonValue>
#include <QHash>
#include <QSet>

#include <algorithm>
#include <memory>

extern "C" {
#include <lauxlib.h>
#include <lua.h>
#include <lualib.h>
}

namespace {
constexpr int kInstructionLimit = 200000;
constexpr int kMaxResultLength = 256 * 1024;
constexpr int kMaxTableDepth = 16;
constexpr int kMaxConstantDepth = 32;
constexpr int kMaxConstantEntries = 10000;
constexpr int kFrozenValueStackReserve = 8;
constexpr qsizetype kMaxCachedRuntimes = 32;
int kConstantProxyMarkerKey;
int kConstantProxyBackingKey;

void instructionHook(lua_State *state, lua_Debug *)
{
    luaL_error(state, "Lua instruction limit exceeded.");
}

void clearGlobal(lua_State *state, const char *name)
{
    lua_pushnil(state);
    lua_setglobal(state, name);
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

    clearGlobal(state, "collectgarbage");
    clearGlobal(state, "dofile");
    clearGlobal(state, "load");
    clearGlobal(state, "loadfile");
    clearGlobal(state, "print");
    clearGlobal(state, "rawset");

    lua_pushliteral(state, "");
    if (lua_getmetatable(state, -1) != 0) {
        lua_pushliteral(state, "protected");
        lua_setfield(state, -2, "__metatable");
        lua_pop(state, 1);
    }
    lua_pop(state, 1);
}

QString luaString(lua_State *state, int index)
{
    size_t length = 0;
    const char *data = lua_tolstring(state, index, &length);
    return data ? QString::fromUtf8(data, qsizetype(length)) : QString();
}

void pushString(lua_State *state, const QByteArray &bytes)
{
    lua_pushlstring(state, bytes.constData(), static_cast<size_t>(bytes.size()));
}

void pushString(lua_State *state, const QString &value)
{
    pushString(state, value.toUtf8());
}

bool isArrayTable(lua_State *state, int index, int &arrayLength)
{
    const int absoluteIndex = lua_absindex(state, index);
    int maxIndex = 0;
    int count = 0;

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

        maxIndex = (std::max)(maxIndex, static_cast<int>(key));
        ++count;
        lua_pop(state, 1);
    }

    arrayLength = maxIndex;
    return count == maxIndex;
}

bool pushConstantBackingTable(lua_State *state, int index)
{
    const int absoluteIndex = lua_absindex(state, index);
    if (!lua_istable(state, absoluteIndex) || lua_getmetatable(state, absoluteIndex) == 0) {
        return false;
    }

    lua_pushlightuserdata(state, &kConstantProxyMarkerKey);
    lua_rawget(state, -2);
    const bool isConstantProxy = lua_toboolean(state, -1) != 0;
    lua_pop(state, 1);
    if (!isConstantProxy) {
        lua_pop(state, 1);
        return false;
    }

    lua_pushlightuserdata(state, &kConstantProxyBackingKey);
    lua_rawget(state, -2);
    lua_remove(state, -2);
    if (lua_istable(state, -1)) {
        return true;
    }
    lua_pop(state, 1);
    return false;
}

QJsonValue luaValueToJson(lua_State *state, int index, int depth, QString &error)
{
    if (depth > kMaxTableDepth) {
        error = QStringLiteral("Lua result nesting is too deep.");
        return {};
    }

    switch (lua_type(state, index)) {
    case LUA_TNIL:
        return QJsonValue();
    case LUA_TBOOLEAN:
        return QJsonValue(lua_toboolean(state, index) != 0);
    case LUA_TNUMBER:
        return QJsonValue(lua_tonumber(state, index));
    case LUA_TSTRING:
        return QJsonValue(luaString(state, index));
    case LUA_TTABLE: {
        const int absoluteIndex = lua_absindex(state, index);
        if (pushConstantBackingTable(state, absoluteIndex)) {
            const QJsonValue value = luaValueToJson(state, -1, depth, error);
            lua_pop(state, 1);
            return value;
        }
        int arrayLength = 0;
        if (isArrayTable(state, absoluteIndex, arrayLength)) {
            QJsonArray array;
            for (int i = 1; i <= arrayLength; ++i) {
                lua_rawgeti(state, absoluteIndex, i);
                array.append(luaValueToJson(state, -1, depth + 1, error));
                lua_pop(state, 1);
                if (!error.isEmpty()) {
                    return {};
                }
            }
            return array;
        }

        QJsonObject object;
        lua_pushnil(state);
        while (lua_next(state, absoluteIndex) != 0) {
            QString key;
            if (lua_type(state, -2) == LUA_TSTRING || lua_type(state, -2) == LUA_TNUMBER) {
                key = luaString(state, -2);
            } else {
                lua_pop(state, 2);
                error = QStringLiteral("Lua result table keys must be strings or numbers.");
                return {};
            }

            object.insert(key, luaValueToJson(state, -1, depth + 1, error));
            lua_pop(state, 1);
            if (!error.isEmpty()) {
                return {};
            }
        }
        return object;
    }
    default:
        error = QStringLiteral("Lua parse(ctx) returned an unsupported value type.");
        return {};
    }
}

QString luaValueToDisplay(lua_State *state, int index, QString &error)
{
    switch (lua_type(state, index)) {
    case LUA_TNIL:
        return QString();
    case LUA_TBOOLEAN:
        return lua_toboolean(state, index) ? QStringLiteral("true") : QStringLiteral("false");
    case LUA_TNUMBER:
        return QString::number(lua_tonumber(state, index), 'g', 15);
    case LUA_TSTRING:
        return luaString(state, index);
    case LUA_TTABLE: {
        const QJsonValue value = luaValueToJson(state, index, 0, error);
        if (!error.isEmpty()) {
            return QString();
        }
        const QJsonDocument document = value.isArray()
            ? QJsonDocument(value.toArray())
            : QJsonDocument(value.toObject());
        return QString::fromUtf8(document.toJson(QJsonDocument::Indented)).trimmed();
    }
    default:
        error = QStringLiteral("Lua parse(ctx) returned an unsupported value type.");
        return QString();
    }
}

void pushContext(lua_State *state, const LuaScriptContext &context)
{
    lua_newtable(state);

    pushString(state, context.topic);
    lua_setfield(state, -2, "topic");

    pushString(state, context.payloadBytes);
    lua_setfield(state, -2, "payload");

    pushString(state, context.payloadBytes.toBase64());
    lua_setfield(state, -2, "payloadBase64");

    pushString(state, context.payloadBytes.toHex(' ').toUpper());
    lua_setfield(state, -2, "payloadHex");

    pushString(state, context.decodedPayload);
    lua_setfield(state, -2, "decoded");

    pushString(state, context.decodeError);
    lua_setfield(state, -2, "decodeError");

    pushString(state, PayloadCodec::formatName(context.format));
    lua_setfield(state, -2, "format");

    pushString(state, context.timestamp);
    lua_setfield(state, -2, "timestamp");
}

struct CachedRuntime {
    ~CachedRuntime()
    {
        if (state) {
            lua_close(state);
        }
    }

    lua_State *state = nullptr;
    int constantsReference = LUA_NOREF;
    QByteArray bytecode;
    QString code;
    quint64 lastUsed = 0;
};

int dumpBytecode(lua_State *, const void *data, size_t size, void *userData)
{
    auto *bytecode = static_cast<QByteArray *>(userData);
    bytecode->append(static_cast<const char *>(data), static_cast<qsizetype>(size));
    return 0;
}

void pushShallowTableCopy(lua_State *state, int index)
{
    const int sourceIndex = lua_absindex(state, index);
    lua_newtable(state);
    const int targetIndex = lua_absindex(state, -1);

    lua_pushnil(state);
    while (lua_next(state, sourceIndex) != 0) {
        lua_pushvalue(state, -2);
        lua_pushvalue(state, -2);
        lua_rawset(state, targetIndex);
        lua_pop(state, 1);
    }
}

void pushExecutionEnvironment(lua_State *state)
{
    lua_pushglobaltable(state);
    const int globalsIndex = lua_absindex(state, -1);
    lua_newtable(state);
    const int environmentIndex = lua_absindex(state, -1);

    lua_pushnil(state);
    while (lua_next(state, globalsIndex) != 0) {
        const bool isGlobalSelf = lua_type(state, -2) == LUA_TSTRING
            && luaString(state, -2) == QStringLiteral("_G");
        if (!isGlobalSelf) {
            lua_pushvalue(state, -2);
            if (lua_istable(state, -2)) {
                pushShallowTableCopy(state, -2);
            } else {
                lua_pushvalue(state, -2);
            }
            lua_rawset(state, environmentIndex);
        }
        lua_pop(state, 1);
    }

    lua_pushvalue(state, environmentIndex);
    lua_setfield(state, environmentIndex, "_G");
    lua_remove(state, globalsIndex);
}

bool setChunkEnvironment(lua_State *state, int chunkIndex, int environmentIndex)
{
    const int absoluteChunkIndex = lua_absindex(state, chunkIndex);
    lua_pushvalue(state, environmentIndex);
    return lua_setupvalue(state, absoluteChunkIndex, 1) != nullptr;
}

int readOnlyNewIndex(lua_State *state)
{
    return luaL_error(state, "Lua constants are read-only.");
}

int readOnlyLength(lua_State *state)
{
    lua_pushinteger(state, static_cast<lua_Integer>(lua_rawlen(state, lua_upvalueindex(1))));
    return 1;
}

int readOnlyNext(lua_State *state)
{
    lua_settop(state, 2);
    lua_pushvalue(state, 2);
    if (lua_next(state, lua_upvalueindex(1)) != 0) {
        return 2;
    }
    return 0;
}

int readOnlyPairs(lua_State *state)
{
    lua_pushvalue(state, lua_upvalueindex(1));
    lua_pushcclosure(state, readOnlyNext, 1);
    lua_pushnil(state);
    lua_pushnil(state);
    return 3;
}

bool pushFrozenValue(
    lua_State *state,
    int index,
    int depth,
    int &entryCount,
    QSet<const void *> &ancestors,
    QString &error)
{
    if (depth > kMaxConstantDepth) {
        error = QStringLiteral("Lua constants nesting is too deep.");
        return false;
    }
    if (lua_checkstack(state, kFrozenValueStackReserve) == 0) {
        error = QStringLiteral("Unable to reserve Lua stack space for constants.");
        return false;
    }

    const int absoluteIndex = lua_absindex(state, index);
    switch (lua_type(state, absoluteIndex)) {
    case LUA_TNIL:
    case LUA_TBOOLEAN:
    case LUA_TNUMBER:
    case LUA_TSTRING:
        lua_pushvalue(state, absoluteIndex);
        return true;
    case LUA_TTABLE: {
        const void *tableIdentity = lua_topointer(state, absoluteIndex);
        if (ancestors.contains(tableIdentity)) {
            error = QStringLiteral("Lua constants must not contain cycles.");
            return false;
        }
        ancestors.insert(tableIdentity);

        lua_newtable(state);
        const int backingIndex = lua_absindex(state, -1);
        lua_pushnil(state);
        while (lua_next(state, absoluteIndex) != 0) {
            const int keyType = lua_type(state, -2);
            if (keyType != LUA_TSTRING && keyType != LUA_TNUMBER) {
                ancestors.remove(tableIdentity);
                error = QStringLiteral("Lua constants table keys must be strings or numbers.");
                return false;
            }
            if (++entryCount > kMaxConstantEntries) {
                ancestors.remove(tableIdentity);
                error = QStringLiteral("Lua constants contain too many entries.");
                return false;
            }

            lua_pushvalue(state, -2);
            if (!pushFrozenValue(state, -2, depth + 1, entryCount, ancestors, error)) {
                ancestors.remove(tableIdentity);
                return false;
            }
            lua_rawset(state, backingIndex);
            lua_pop(state, 1);
        }
        ancestors.remove(tableIdentity);

        lua_newtable(state);
        const int proxyIndex = lua_absindex(state, -1);
        lua_newtable(state);

        lua_pushvalue(state, backingIndex);
        lua_setfield(state, -2, "__index");
        lua_pushcfunction(state, readOnlyNewIndex);
        lua_setfield(state, -2, "__newindex");
        lua_pushvalue(state, backingIndex);
        lua_pushcclosure(state, readOnlyLength, 1);
        lua_setfield(state, -2, "__len");
        lua_pushvalue(state, backingIndex);
        lua_pushcclosure(state, readOnlyPairs, 1);
        lua_setfield(state, -2, "__pairs");
        lua_pushlightuserdata(state, &kConstantProxyBackingKey);
        lua_pushvalue(state, backingIndex);
        lua_rawset(state, -3);
        lua_pushlightuserdata(state, &kConstantProxyMarkerKey);
        lua_pushboolean(state, true);
        lua_rawset(state, -3);
        lua_pushliteral(state, "protected");
        lua_setfield(state, -2, "__metatable");
        lua_setmetatable(state, proxyIndex);

        lua_remove(state, backingIndex);
        return true;
    }
    default:
        error = QStringLiteral(
            "Lua constants may only contain nil, booleans, numbers, strings, and tables.");
        return false;
    }
}

std::shared_ptr<CachedRuntime> createRuntime(const QString &code, QString &error)
{
    auto runtime = std::make_shared<CachedRuntime>();
    runtime->state = luaL_newstate();
    if (!runtime->state) {
        error = QStringLiteral("Unable to create Lua state.");
        return {};
    }

    lua_State *state = runtime->state;
    openSafeLibraries(state);
    lua_sethook(state, instructionHook, LUA_MASKCOUNT, kInstructionLimit);

    pushExecutionEnvironment(state);
    const int environmentIndex = lua_absindex(state, -1);
    const QByteArray scriptBytes = code.toUtf8();
    if (luaL_loadbuffer(
            state,
            scriptBytes.constData(),
            static_cast<size_t>(scriptBytes.size()),
            "mqtt-plus-script") != LUA_OK) {
        error = luaString(state, -1);
        return {};
    }
    const int chunkIndex = lua_absindex(state, -1);
    if (lua_dump(state, dumpBytecode, &runtime->bytecode, 0) != 0) {
        error = QStringLiteral("Unable to cache compiled Lua bytecode.");
        return {};
    }
    if (!setChunkEnvironment(state, chunkIndex, environmentIndex)) {
        error = QStringLiteral("Unable to isolate the Lua script environment.");
        return {};
    }

    if (lua_pcall(state, 0, 0, 0) != LUA_OK) {
        error = luaString(state, -1);
        return {};
    }

    lua_getfield(state, environmentIndex, "parse");
    if (!lua_isfunction(state, -1)) {
        error = QStringLiteral("Lua script must define function parse(ctx).");
        return {};
    }
    lua_pop(state, 1);

    lua_getfield(state, environmentIndex, "constants");
    if (lua_isnil(state, -1)) {
        lua_pop(state, 1);
        lua_newtable(state);
    } else if (lua_isfunction(state, -1)) {
        lua_sethook(state, instructionHook, LUA_MASKCOUNT, kInstructionLimit);
        if (lua_pcall(state, 0, 1, 0) != LUA_OK) {
            error = luaString(state, -1);
            return {};
        }
        if (lua_isnil(state, -1)) {
            lua_pop(state, 1);
            lua_newtable(state);
        }
    } else {
        lua_pop(state, 1);
        lua_newtable(state);
    }

    if (!lua_istable(state, -1)) {
        error = QStringLiteral("Lua constants() must return a table or nil.");
        return {};
    }

    int entryCount = 0;
    QSet<const void *> ancestors;
    QString freezeError;
    if (!pushFrozenValue(state, -1, 0, entryCount, ancestors, freezeError)) {
        error = freezeError;
        return {};
    }

    runtime->constantsReference = luaL_ref(state, LUA_REGISTRYINDEX);
    runtime->code = code;
    lua_sethook(state, nullptr, 0, 0);
    lua_settop(state, 0);
    return runtime;
}

LuaScriptResult runIsolatedRuntime(CachedRuntime &runtime, const LuaScriptContext &context)
{
    LuaScriptResult result;
    lua_State *state = runtime.state;
    lua_settop(state, 0);
    lua_sethook(state, instructionHook, LUA_MASKCOUNT, kInstructionLimit);

    pushExecutionEnvironment(state);
    const int environmentIndex = lua_absindex(state, -1);
    if (luaL_loadbufferx(
            state,
            runtime.bytecode.constData(),
            static_cast<size_t>(runtime.bytecode.size()),
            "mqtt-plus-script",
            "b") != LUA_OK) {
        lua_sethook(state, nullptr, 0, 0);
        result.error = luaString(state, -1);
        lua_settop(state, 0);
        return result;
    }
    const int chunkIndex = lua_absindex(state, -1);
    if (!setChunkEnvironment(state, chunkIndex, environmentIndex)) {
        lua_sethook(state, nullptr, 0, 0);
        result.error = QStringLiteral("Unable to isolate the Lua script environment.");
        lua_settop(state, 0);
        return result;
    }
    if (lua_pcall(state, 0, 0, 0) != LUA_OK) {
        lua_sethook(state, nullptr, 0, 0);
        result.error = luaString(state, -1);
        lua_settop(state, 0);
        return result;
    }

    lua_getfield(state, environmentIndex, "parse");
    if (!lua_isfunction(state, -1)) {
        lua_sethook(state, nullptr, 0, 0);
        result.error = QStringLiteral("Lua script must define function parse(ctx).");
        lua_settop(state, 0);
        return result;
    }
    pushContext(state, context);
    lua_rawgeti(state, LUA_REGISTRYINDEX, runtime.constantsReference);

    if (lua_pcall(state, 2, 1, 0) != LUA_OK) {
        lua_sethook(state, nullptr, 0, 0);
        result.error = luaString(state, -1);
        lua_settop(state, 0);
        return result;
    }
    lua_sethook(state, nullptr, 0, 0);

    QString conversionError;
    result.output = luaValueToDisplay(state, -1, conversionError);
    lua_settop(state, 0);
    if (!conversionError.isEmpty()) {
        result.error = conversionError;
        return result;
    }
    if (result.output.size() > kMaxResultLength) {
        result.error = QStringLiteral("Lua result exceeds the maximum display length.");
        return result;
    }

    result.success = true;
    return result;
}
}

namespace LuaRunner {

struct RuntimeCache::Impl {
    void evictLeastRecentlyUsed()
    {
        if (runtimes.size() < kMaxCachedRuntimes) {
            return;
        }

        auto victim = runtimes.begin();
        for (auto it = runtimes.begin(); it != runtimes.end(); ++it) {
            if ((*it)->lastUsed < (*victim)->lastUsed) {
                victim = it;
            }
        }
        runtimes.erase(victim);
    }

    QHash<QString, std::shared_ptr<CachedRuntime>> runtimes;
    quint64 useCounter = 0;
};

RuntimeCache::RuntimeCache()
    : m_impl(std::make_unique<Impl>())
{
}

RuntimeCache::~RuntimeCache() = default;

LuaScriptResult RuntimeCache::run(
    const QString &scriptId,
    const QString &code,
    const LuaScriptContext &context)
{
    LuaScriptResult result;
    auto runtimeIt = m_impl->runtimes.find(scriptId);
    if (runtimeIt != m_impl->runtimes.end() && (*runtimeIt)->code != code) {
        m_impl->runtimes.erase(runtimeIt);
        runtimeIt = m_impl->runtimes.end();
    }

    std::shared_ptr<CachedRuntime> runtime;
    if (runtimeIt == m_impl->runtimes.end()) {
        QString error;
        runtime = createRuntime(code, error);
        if (!runtime) {
            result.error = error;
            return result;
        }
        m_impl->evictLeastRecentlyUsed();
        runtimeIt = m_impl->runtimes.insert(scriptId, runtime);
    } else {
        runtime = *runtimeIt;
    }

    runtime->lastUsed = ++m_impl->useCounter;
    return runIsolatedRuntime(*runtime, context);
}

void RuntimeCache::clear()
{
    m_impl->runtimes.clear();
    m_impl->useCounter = 0;
}

} // namespace LuaRunner
