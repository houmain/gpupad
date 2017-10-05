#ifndef SCRIPTENGINE_H
#define SCRIPTENGINE_H

#include "MessageList.h"
#include <QJSEngine>
#include <QJSValue>

class ScriptEngine
{
public:
    struct Script
    {
        QString fileName;
        QString source;
    };

    ScriptEngine();
    ~ScriptEngine();

    void reset(QList<Script> scripts);
    void setGlobal(const QString &name, QObject *object);
    QJSValue getGlobal(const QString &name);
    QJSValue call(QJSValue& callable, const QJSValueList &args,
        ItemId itemId, MessagePtrSet &messages);
    QStringList evaluateValue(const QStringList &fieldExpressions,
        ItemId itemId, MessagePtrSet &messages);

    template<typename T>
    QJSValue toJsValue(const T &value) { return mJsEngine->toScriptValue(value); }

private:
    QJSValue evaluate(const QString &program, const QString &fileName = QString());
    void reset();

    QScopedPointer<QJSEngine> mJsEngine;
    QList<Script> mScripts;
    MessagePtrSet mMessages;
};

#endif // SCRIPTENGINE_H
