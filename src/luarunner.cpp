#include "luarunner.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonValue>

#include <algorithm>

extern "C" {
#include <lauxlib.h>
#include <lua.h>
#include <lualib.h>
}

namespace {
constexpr int kInstructionLimit = 200000;
constexpr int kMaxResultLength = 256 * 1024;
constexpr int kMaxTableDepth = 16;

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
}

LuaScriptResult LuaRunner::run(const QString &code, const LuaScriptContext &context)
{
    LuaScriptResult result;
    lua_State *state = luaL_newstate();
    if (!state) {
        result.error = QStringLiteral("Unable to create Lua state.");
        return result;
    }

    openSafeLibraries(state);
    lua_sethook(state, instructionHook, LUA_MASKCOUNT, kInstructionLimit);

    const QByteArray scriptBytes = code.toUtf8();
    if (luaL_loadbuffer(
            state,
            scriptBytes.constData(),
            static_cast<size_t>(scriptBytes.size()),
            "mqtt-plus-script") != LUA_OK) {
        result.error = luaString(state, -1);
        lua_close(state);
        return result;
    }

    if (lua_pcall(state, 0, 0, 0) != LUA_OK) {
        result.error = luaString(state, -1);
        lua_close(state);
        return result;
    }

    lua_getglobal(state, "parse");
    if (!lua_isfunction(state, -1)) {
        result.error = QStringLiteral("Lua script must define function parse(ctx).");
        lua_close(state);
        return result;
    }

    pushContext(state, context);
    if (lua_pcall(state, 1, 1, 0) != LUA_OK) {
        result.error = luaString(state, -1);
        lua_close(state);
        return result;
    }

    QString conversionError;
    result.output = luaValueToDisplay(state, -1, conversionError);
    if (!conversionError.isEmpty()) {
        result.error = conversionError;
        lua_close(state);
        return result;
    }
    if (result.output.size() > kMaxResultLength) {
        result.error = QStringLiteral("Lua result exceeds the maximum display length.");
        lua_close(state);
        return result;
    }

    result.success = true;
    lua_close(state);
    return result;
}
