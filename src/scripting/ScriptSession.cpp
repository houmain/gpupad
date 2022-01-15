#include "ScriptSession.h"
#include "SessionScriptObject.h"
#include "MouseScriptObject.h"
#include "KeyboardScriptObject.h"
#include "Singletons.h"

ScriptSession::ScriptSession(QObject *parent)
    : QObject(parent)
    , mMouseScriptObject(new MouseScriptObject(this))
    , mKeyboardScriptObject(new KeyboardScriptObject(this))
{
}

void ScriptSession::prepare()
{
    Q_ASSERT(onMainThread());
    Singletons::inputState().update();
    mMouseScriptObject->update(Singletons::inputState());
    mKeyboardScriptObject->update(Singletons::inputState());
}

void ScriptSession::beginSessionUpdate(SessionModel *sessionCopy)
{
    Q_ASSERT(!onMainThread());
    if (!mScriptEngine)
        initializeEngine();
    mSessionScriptObject->beginBackgroundUpdate(sessionCopy);
}

void ScriptSession::endSessionUpdate()
{
    Q_ASSERT(onMainThread());
    mSessionScriptObject->endBackgroundUpdate();
}

bool ScriptSession::usesMouseState() const
{
    return mMouseScriptObject->wasRead();
}

bool ScriptSession::usesKeyboardState() const
{
    return mKeyboardScriptObject->wasRead();
}

ScriptEngine& ScriptSession::engine()
{
    Q_ASSERT(mScriptEngine);
    return *mScriptEngine;
}

void ScriptSession::initializeEngine()
{
    Q_ASSERT(!mScriptEngine);
    mScriptEngine.reset(new ScriptEngine());
    mScriptEngine->setTimeout(5000);
    mSessionScriptObject = new SessionScriptObject(mScriptEngine->jsEngine());
    mScriptEngine->setGlobal("Session", mSessionScriptObject);
    mScriptEngine->setGlobal("Mouse", mMouseScriptObject);
    mScriptEngine->setGlobal("Keyboard", mKeyboardScriptObject);
}
