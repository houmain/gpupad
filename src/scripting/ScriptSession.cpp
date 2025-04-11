#include "ScriptSession.h"
#include "ScriptEngine.h"
#include "scripting/IScriptRenderSession.h"
#include "objects/AppScriptObject.h"
#include "objects/SessionScriptObject.h"
#include "Singletons.h"

ScriptSession::ScriptSession(IScriptRenderSession *renderSession,
    QObject *parent)
    : QObject(parent)
    , mRenderSession(*renderSession)
{
    moveToThread(mRenderSession.renderThread());
    mScriptEngine = ScriptEngine::make(mRenderSession.basePath(), this);
    Q_ASSERT(mScriptEngine->thread() == mRenderSession.renderThread());
}

void ScriptSession::beginSessionUpdate()
{
    Q_ASSERT(!onMainThread());
    mScriptEngine->appScriptObject()
        .sessionScriptObject()
        .beginBackgroundUpdate(&mRenderSession);
}

ScriptEngine &ScriptSession::engine()
{
    Q_ASSERT(!onMainThread());
    return *mScriptEngine;
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
    Q_ASSERT(onMainThread());
    return mScriptEngine->appScriptObject().usesMouseState();
}

bool ScriptSession::usesKeyboardState() const
{
    Q_ASSERT(onMainThread());
    return mScriptEngine->appScriptObject().usesKeyboardState();
}

void ScriptSession::updateInputState()
{
    Q_ASSERT(onMainThread());
    mScriptEngine->appScriptObject().updateInputState();
}