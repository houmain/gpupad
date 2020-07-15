#ifndef SCRIPTENGINE_H
#define SCRIPTENGINE_H

#include "MessageList.h"
#include <QJSEngine>
#include <QJSValue>

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
    QList<double> evaluateValues(const QStringList &valueExpressions,
        ItemId itemId, MessagePtrSet &messages);
    double evaluateValue(const QString &valueExpression,
      ItemId itemId, MessagePtrSet &messages);

    template<typename T>
    QJSValue toJsValue(const T &value) { return mJsEngine->toScriptValue(value); }

private:
    QScopedPointer<QJSEngine> mJsEngine;
    MessagePtrSet mMessages;
};

#endif // SCRIPTENGINE_H
