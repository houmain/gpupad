#pragma once

#include "MessageList.h"
#include "session/Item.h"
#include <QObject>

using ScriptEnginePtr = std::shared_ptr<class ScriptEngine>;
class IScriptRenderSession;

class ScriptSession : public QObject
{
    Q_OBJECT
public:
    explicit ScriptSession(const QString &basePath, QObject *parent = nullptr);

    // 1. called in main thread
    bool usesMouseState() const;
    bool usesKeyboardState() const;
    void updateInputState();
    MessagePtrSet resetMessages() { return std::exchange(mMessages, {}); }

    // 2. called in render thread
    void beginSessionUpdate(IScriptRenderSession *renderSession);
    ScriptEngine &engine();

    // 3. called in main thread
    void endSessionUpdate();

private:
    QString mBasePath;
    MessagePtrSet mMessages;
    ScriptEnginePtr mScriptEngine;
};
