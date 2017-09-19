#ifndef SCRIPTENGINE_H
#define SCRIPTENGINE_H

#include "MessageList.h"
#include <QJsonArray>

class QJSEngine;

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
    QVariant evaluate(const QString &expression,
        ItemId itemId, MessagePtrSet &messages);
    QStringList evaluateValue(const QStringList &fieldExpressions,
        ItemId itemId, MessagePtrSet &messages);

private:
    void reset();

    QScopedPointer<QJSEngine> mJsEngine;
    QList<Script> mScripts;
    MessagePtrSet mMessages;
};

#endif // SCRIPTENGINE_H
