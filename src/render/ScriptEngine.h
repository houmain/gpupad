#ifndef SCRIPTENGINE_H
#define SCRIPTENGINE_H

#include "MessageList.h"

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

    void evalScripts(QList<Script> scripts);
    QStringList evalValue(const QStringList &fieldExpressions,
        ItemId itemId, MessagePtrSet &messages);

private:
    void reset();

    QScopedPointer<QJSEngine> mJsEngine;
    QList<Script> mScripts;
    MessagePtrSet mMessages;
};

#endif // SCRIPTENGINE_H
