#include "ScriptSession.h"
#include "ScriptEngine.h"
#include "objects/AppScriptObject.h"
#include "objects/SessionScriptObject.h"
#include "Singletons.h"

ScriptSession::ScriptSession(const QString &basePath, QObject *parent)
    : QObject(parent)
    , mBasePath(basePath)
{
}

void ScriptSession::prepare()
{
    Q_ASSERT(onMainThread());
    if (mScriptEngine)
        mScriptEngine->appScriptObject().update();
}

void ScriptSession::beginSessionUpdate(SessionModel *sessionCopy)
{
    Q_ASSERT(!onMainThread());
    if (!mScriptEngine)
        mScriptEngine = ScriptEngine::make(mBasePath);

    mScriptEngine->appScriptObject()
        .sessionScriptObject()
        .beginBackgroundUpdate(sessionCopy);
}

void ScriptSession::endSessionUpdate()
{
    Q_ASSERT(onMainThread());
    mScriptEngine->appScriptObject()
        .sessionScriptObject()
        .endBackgroundUpdate();
}

bool ScriptSession::usesMouseState() const
{
    return mScriptEngine->appScriptObject().usesMouseState();
}

bool ScriptSession::usesKeyboardState() const
{
    return mScriptEngine->appScriptObject().usesKeyboardState();
}

ScriptEngine &ScriptSession::engine()
{
    Q_ASSERT(mScriptEngine);
    return *mScriptEngine;
}
