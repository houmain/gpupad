#include "ScriptSession.h"
#include "SessionScriptObject.h"
#include "MouseScriptObject.h"
#include "KeyboardScriptObject.h"
#include "Singletons.h"
#include "ScriptEngineJavaScript.h"

ScriptSession::ScriptSession(SourceType sourceType, QObject *parent)
    : QObject(parent)
    , mSourceType(sourceType)
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
    if (mSessionScriptObject)
        mSessionScriptObject->beginBackgroundUpdate(sessionCopy);
}

void ScriptSession::endSessionUpdate()
{
    Q_ASSERT(onMainThread());
    if (mSessionScriptObject)
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
    auto scriptEngine = new ScriptEngineJavaScript();
    mScriptEngine.reset(scriptEngine);
    mSessionScriptObject = new SessionScriptObject(scriptEngine->jsEngine());
    mScriptEngine->setGlobal("Session", mSessionScriptObject);
    mScriptEngine->setTimeout(5000);
    mScriptEngine->setGlobal("Mouse", mMouseScriptObject);
    mScriptEngine->setGlobal("Keyboard", mKeyboardScriptObject);
}
