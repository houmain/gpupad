#include "ScriptSession.h"
#include "SessionScriptObject.h"
#include "MouseScriptObject.h"
#include "KeyboardScriptObject.h"
#include "Singletons.h"

ScriptSession::ScriptSession(QObject *parent)
    : QObject(parent)   
    , mSessionScriptObject(new SessionScriptObject(this))
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
    mSessionScriptObject->beginUpdate(&engine(), sessionCopy);
}

void ScriptSession::endSessionUpdate()
{
    Q_ASSERT(!onMainThread());
    mSessionScriptObject->endUpdate();
}

void ScriptSession::applySessionUpdate()
{
    Q_ASSERT(onMainThread());
    mSessionScriptObject->applyUpdate();
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
    if (!mScriptEngine)
        initializeEngine();
    return *mScriptEngine;
}

void ScriptSession::initializeEngine()
{
    Q_ASSERT(!mScriptEngine);
    mScriptEngine = new ScriptEngine(this);
    mScriptEngine->setTimeout(5000);
    mScriptEngine->setGlobal("Mouse", mMouseScriptObject);
    mScriptEngine->setGlobal("Keyboard", mKeyboardScriptObject);
}
