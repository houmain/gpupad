#include "ScriptSession.h"
#include "ScriptEngine.h"
#include "scripting/IScriptRenderSession.h"
#include "objects/AppScriptObject.h"
#include "objects/SessionScriptObject.h"
#include "Singletons.h"
#include <QThread>

ScriptSession::ScriptSession(const QString &basePath, QObject *parent)
    : QObject(parent)
    , mBasePath(basePath)
{
    Q_ASSERT(!onMainThread());
    moveToThread(QThread::currentThread());
    mScriptEngine = ScriptEngine::make(mBasePath, this);
}

void ScriptSession::beginSessionUpdate(IScriptRenderSession *renderSession)
{
    Q_ASSERT(!onMainThread());
    mScriptEngine->appScriptObject()
        .sessionScriptObject()
        .beginBackgroundUpdate(renderSession);
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