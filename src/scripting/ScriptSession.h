#pragma once

#include "MessageList.h"
#include "SourceType.h"
#include <QObject>

using ScriptEnginePtr = std::shared_ptr<class ScriptEngine>;
class SessionModel;

class ScriptSession : public QObject
{
    Q_OBJECT
public:
    explicit ScriptSession(const QString &basePath, QObject *parent = nullptr);

    // 1. called in main thread
    void prepare();
    bool usesMouseState() const;
    bool usesKeyboardState() const;
    MessagePtrSet resetMessages() { return std::exchange(mMessages, {}); }

    // 2. called in render thread
    void beginSessionUpdate(SessionModel *sessionCopy);
    ScriptEngine &engine();

    // 3. called in main thread
    void endSessionUpdate();

private:
    QString mBasePath;
    MessagePtrSet mMessages;
    ScriptEnginePtr mScriptEngine;
};
