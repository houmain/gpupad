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

        friend bool operator==(const Script &a, const Script &b)
        {
            return (a.fileName == b.fileName &&
                    a.source == b.source);
        }
    };

    explicit ScriptEngine(QList<Script> scripts);
    ~ScriptEngine();

    const QList<Script> &scripts() const { return mScripts; }
    void setGlobal(const QString &name, QObject *object);
    QJSValue getGlobal(const QString &name);
    QJSValue call(QJSValue &callable, const QJSValueList &args,
        ItemId itemId, MessagePtrSet &messages);
    QStringList evaluateValues(const QStringList &valueExpressions,
        ItemId itemId, MessagePtrSet &messages);

    template<typename T>
    QJSValue toJsValue(const T &value) { return mJsEngine->toScriptValue(value); }

private:
    QJSValue evaluate(const QString &program, const QString &fileName = QString());

    const QList<Script> mScripts;
    QScopedPointer<QJSEngine> mJsEngine;
    MessagePtrSet mMessages;
};

#endif // SCRIPTENGINE_H
