#ifndef SCRIPTENGINE_H
#define SCRIPTENGINE_H

#include "MessageList.h"
#include <QJSEngine>
#include <QJSValue>

using ScriptValue = double;
using ScriptValueList = QList<ScriptValue>;

class ScriptVariable
{
public:
    int count() const;
    ScriptValue get() const;
    ScriptValue get(int index) const;

private:
    friend class ScriptEngine;
    QSharedPointer<ScriptValueList> mValues;
};

class ScriptEngine
{
public:
    ScriptEngine();
    ~ScriptEngine();

    void setGlobal(const QString &name, QJSValue value);
    void setGlobal(const QString &name, QObject *object);
    QJSValue getGlobal(const QString &name);
    QJSValue call(QJSValue &callable, const QJSValueList &args,
        ItemId itemId, MessagePtrSet &messages);
    void evaluateScript(const QString &script, const QString &fileName);
    void evaluateExpression(const QString &script, const QString &resultName, 
        ItemId itemId, MessagePtrSet &messages);
    ScriptValueList evaluateValues(const QStringList &valueExpressions,
        ItemId itemId, MessagePtrSet &messages);
    ScriptValue evaluateValue(const QString &valueExpression,
      ItemId itemId, MessagePtrSet &messages);

    void updateVariables();
    ScriptVariable getVariable(const QStringList &valueExpressions,
        ItemId itemId, MessagePtrSet &messages);
    ScriptVariable getVariable(const QString &valueExpression,
        ItemId itemId, MessagePtrSet &messages);

    template<typename T>
    QJSValue toJsValue(const T &value) { return mJsEngine->toScriptValue(value); }

private:
    QScopedPointer<QJSEngine> mJsEngine;
    MessagePtrSet mMessages;
    QList<QPair<QStringList, QWeakPointer<ScriptValueList>>> mVariables;
};

#endif // SCRIPTENGINE_H
