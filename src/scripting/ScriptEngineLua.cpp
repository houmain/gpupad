
#include "ScriptEngineLua.h"
#include "lua.hpp"
#include <QRegularExpression>
#include <cstring>

namespace
{
    int messageHandler(lua_State *L) 
    {
        if (lua_tostring(L, 1))
            return 1;
        
        if (luaL_callmeta(L, 1, "__tostring") &&
            lua_type(L, -1) == LUA_TSTRING) 
            return 1;

        lua_pushfstring(L, "(error object is a %s value)", 
            luaL_typename(L, 1));
        return 1;
    }

    int doCall(lua_State *L, int narg, int nres) 
    {
        const auto base = lua_gettop(L) - narg;
        lua_pushcfunction(L, messageHandler);
        lua_insert(L, base);
        const auto status = lua_pcall(L, narg, nres, base);
        lua_remove(L, base);
        return status;
    }

    int loadBuffer(lua_State *L, const char *string, const char *name)
    {
        return luaL_loadbuffer(L, string, std::strlen(string), name);
    }
} // namespace

ScriptEngineLua::ScriptEngineLua(QObject *parent)
    : ScriptEngine(parent)
    , mLuaState(luaL_newstate())
{
    luaL_openlibs(mLuaState);
}

ScriptEngineLua::~ScriptEngineLua()
{
    lua_close(mLuaState);
}

void ScriptEngineLua::setTimeout(int msec)
{
}

void ScriptEngineLua::setGlobal(const QString &name, QObject *object)
{
    // TODO: push user object
}

void ScriptEngineLua::setGlobal(const QString &name, const ScriptValueList &values)
{
    if (values.isEmpty()) {
        lua_pushnil(mLuaState);
    }
    else if (values.size() == 1) {
        lua_pushnumber(mLuaState, values.first());
    }
    else {
        lua_createtable(mLuaState, values.size(), 0);
        const auto table = lua_gettop(mLuaState);
        auto i = 1;
        for (const auto& value : values) {
          lua_pushnumber(mLuaState, i++);
          lua_pushnumber(mLuaState, value);
          lua_rawset(mLuaState, table);
        }
    }
    lua_setglobal(mLuaState, qUtf8Printable(name));
}

void ScriptEngineLua::validateScript(const QString &script,
    const QString &fileName, MessagePtrSet &messages)
{
    if (loadBuffer(mLuaState, qUtf8Printable(script), 
                qUtf8Printable(fileName)) != LUA_OK)
        popErrorMessage(messages);
}

void ScriptEngineLua::evaluateScript(const QString &script,
    const QString &fileName, MessagePtrSet &messages)
{
    if (loadBuffer(mLuaState, qUtf8Printable(script), 
                qUtf8Printable(fileName)) != LUA_OK || 
            doCall(mLuaState, 0, 0) != LUA_OK)
        popErrorMessage(messages);
}

ScriptValueList ScriptEngineLua::evaluateValues(const QString &valueExpression,
        ItemId itemId, MessagePtrSet &messages) 
{
   if (loadBuffer(mLuaState, qUtf8Printable("return " + valueExpression), "") != LUA_OK || 
            doCall(mLuaState, 0, 1) != LUA_OK) {
        popErrorMessage(messages, itemId);
        return { };
    }
    auto values = ScriptValueList();
    if (lua_istable(mLuaState, -1)) {
        const auto table = lua_gettop(mLuaState);
        lua_pushnil(mLuaState);
        while (lua_next(mLuaState, table)) {
            values.append(lua_tonumber(mLuaState, -1));
            lua_pop(mLuaState, 1); // remove value, keep key
        }
    }
    else {
        values.push_back(lua_tonumber(mLuaState, -1));
    }
    lua_pop(mLuaState, 1);
    return values;
}

void ScriptEngineLua::interrupt() 
{
  auto flag = LUA_MASKCALL | LUA_MASKRET | LUA_MASKLINE | LUA_MASKCOUNT;
  lua_sethook(mLuaState, 
      [](lua_State *L, lua_Debug *) {
          lua_sethook(L, nullptr, 0, 0);
          luaL_error(L, "interrupted!");
      }, flag, 1);
}

void ScriptEngineLua::popErrorMessage(MessagePtrSet &messages, ItemId itemId)
{
    const auto message = QString::fromUtf8(lua_tostring(mLuaState, -1));
    lua_pop(mLuaState, 1);

    //[string "FILENAME"]:LINE: MESSAGE
    static const auto split = QRegularExpression(
        "\\[string \""
        "([^\"]*)\""   // 1. filename
        "\\]:(\\d+): " // 2. line
        "(.+)");       // 3. text

    const auto lines = message.split('\n');
    for (const auto &line : lines) {
        const auto match = split.match(line);
        const auto fileName = match.captured(1);
        const auto lineNumber = match.captured(2).toInt();
        const auto text = match.captured(3);
        messages += (itemId ? 
            MessageList::insert(itemId,
                MessageType::ScriptError, text) :
            MessageList::insert(fileName, lineNumber,
                MessageType::ScriptError, text));
    }
}

