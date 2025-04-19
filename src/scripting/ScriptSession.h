#pragma once

#include "MessageList.h"
#include <QObject>

using ScriptEnginePtr = std::shared_ptr<class ScriptEngine>;
class IScriptRenderSession;

class ScriptSession final : public QObject
{
    Q_OBJECT
public:
    ScriptSession(IScriptRenderSession *renderSession,
        QObject *parent = nullptr);
    void resetEngine();

    // 1. called in main thread
    bool usesMouseState() const;
    bool usesKeyboardState() const;
    void update();
    MessagePtrSet resetMessages() { return std::exchange(mMessages, {}); }

    // 2. called in render thread
    void beginSessionUpdate();
    ScriptEngine &engine();

    // 3. called in main thread
    void endSessionUpdate();

private:
    IScriptRenderSession &mRenderSession;
    MessagePtrSet mMessages;
    ScriptEnginePtr mScriptEngine;
};
