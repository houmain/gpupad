#include "ScriptSession.h"
#include "ScriptEngineJavaScript.h"
#include "objects/AppScriptObject.h"
#include "objects/SessionScriptObject.h"
#include "Singletons.h"

ScriptSession::ScriptSession(SourceType sourceType, QObject *parent)
    : QObject(parent)
    , mSourceType(sourceType)
    , mAppScriptObject(new AppScriptObject(this))
{
}

void ScriptSession::prepare()
{
    Q_ASSERT(onMainThread());
    mAppScriptObject->update();
}

void ScriptSession::beginSessionUpdate(SessionModel *sessionCopy)
{
    Q_ASSERT(!onMainThread());
    if (!mScriptEngine)
        initializeEngine();
    mAppScriptObject->sessionScriptObject().beginBackgroundUpdate(sessionCopy);
}

void ScriptSession::endSessionUpdate()
{
    Q_ASSERT(onMainThread());
    mAppScriptObject->sessionScriptObject().endBackgroundUpdate();
}

bool ScriptSession::usesMouseState() const
{
    return mAppScriptObject->usesMouseState();
}

bool ScriptSession::usesKeyboardState() const
{
    return mAppScriptObject->usesKeyboardState();
}

ScriptEngine &ScriptSession::engine()
{
    Q_ASSERT(mScriptEngine);
    return *mScriptEngine;
}

void ScriptSession::initializeEngine()
{
    Q_ASSERT(!mScriptEngine);
    auto scriptEngine = new ScriptEngineJavaScript();
    mScriptEngine.reset(scriptEngine);
    mScriptEngine->setTimeout(5000);

    mAppScriptObject->initializeEngine(scriptEngine->jsEngine());
    mScriptEngine->setGlobal("app", mAppScriptObject);
}
