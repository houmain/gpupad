#pragma once

#include "ScriptEngine.h"

struct lua_State;

class ScriptEngineLua : public ScriptEngine
{
    Q_OBJECT
public:
    explicit ScriptEngineLua(QObject *parent = nullptr);
    ~ScriptEngineLua();

    void setTimeout(int msec) override;
    void setGlobal(const QString &name, QObject *object) override;
    void setGlobal(const QString &name, const ScriptValueList &values) override;
    void validateScript(const QString &script,
        const QString &fileName, MessagePtrSet &messages) override;
    void evaluateScript(const QString &script,
        const QString &fileName, MessagePtrSet &messages) override;
    ScriptValueList evaluateValues(const QString &valueExpression,
        ItemId itemId, MessagePtrSet &messages) override;

private:
    void popErrorMessage(MessagePtrSet &messages, ItemId itemId = 0);
    void interrupt();

    lua_State *mLuaState{ }; 
};
