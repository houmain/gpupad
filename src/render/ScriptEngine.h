#ifndef SCRIPTENGINE_H
#define SCRIPTENGINE_H

#include "MessageList.h"

class QJSEngine;

class ScriptEngine
{
public:
    ScriptEngine();
    ~ScriptEngine();

    void evalScript(const QString &fileName, ItemId itemId);
    QStringList evalValue(const QStringList &fieldExpressions, ItemId itemId);

private:
    QScopedPointer<QJSEngine> mJsEngine;
    MessagePtrList mMessages;
};

#endif // SCRIPTENGINE_H
