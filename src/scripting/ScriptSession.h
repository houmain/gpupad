#ifndef SCRIPTSESSION_H
#define SCRIPTSESSION_H

#include "ScriptEngine.h"

class SessionModel;
class SessionScriptObject;
class MouseScriptObject;
class KeyboardScriptObject;

class ScriptSession : public QObject
{
    Q_OBJECT

public:
    explicit ScriptSession(QObject *parent = nullptr);

    // 1. called in main thread
    void prepare();
    bool usesMouseState() const;
    bool usesKeyboardState() const;
    MessagePtrSet resetMessages() { return std::exchange(mMessages, {}); }

    // 2. called in render thread
    void beginSessionUpdate(SessionModel *sessionCopy);
    ScriptEngine& engine();

    // 3. called in main thread
    void endSessionUpdate();

private:
    void initializeEngine();

    MessagePtrSet mMessages;

    QScopedPointer<ScriptEngine> mScriptEngine;
    SessionScriptObject *mSessionScriptObject{ };
    MouseScriptObject *mMouseScriptObject{ };
    KeyboardScriptObject *mKeyboardScriptObject{ };
};

#endif // SCRIPTSESSION_H
