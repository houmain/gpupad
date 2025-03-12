#pragma once

#include "MessageList.h"
#include "SourceType.h"
#include <QObject>

class ScriptEngine;
class SessionModel;
class AppScriptObject;

class ScriptSession : public QObject
{
    Q_OBJECT
public:
    explicit ScriptSession(const QString &sessionPath,
        QObject *parent = nullptr);

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
    void initializeEngine();

    MessagePtrSet mMessages;

    std::unique_ptr<ScriptEngine> mScriptEngine;
    AppScriptObject *mAppScriptObject{};
};
