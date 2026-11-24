#include "ScriptSession.h"
#include "ScriptEngine.h"
#include "scripting/IScriptRenderSession.h"
#include "objects/AppScriptObject.h"
#include "Singletons.h"
#include "FileDialog.h"
#include <QThread>

ScriptSession::ScriptSession(IScriptRenderSession *renderSession,
    QObject *parent)
    : QObject(parent)
    , mRenderSession(*renderSession)
{
    const auto globals = Singletons::scriptGlobals();
    mScriptEngine =
        ScriptEngine::make(QDir::current(), mRenderSession.renderThread());
    const auto setGlobals = [engine = mScriptEngine, globals]() {
        for (auto it = globals.cbegin(); it != globals.cend(); ++it)
            engine->setGlobal(it.key(), it.value());
    };
    if (mScriptEngine->thread() == QThread::currentThread())
        setGlobals();
    else
        QMetaObject::invokeMethod(mScriptEngine.get(), setGlobals,
            Qt::BlockingQueuedConnection);
}

void ScriptSession::resetEngine()
{
    Q_ASSERT(mScriptEngine.use_count() == 1);
    mScriptEngine.reset();
}

void ScriptSession::beginSessionUpdate()
{
    Q_ASSERT(!onMainThread());
    mScriptEngine->appScriptObject()
        .beginBackgroundUpdate(&mRenderSession);
}

void ScriptSession::endSessionUpdate()
{
    Q_ASSERT(!onMainThread());
    mScriptEngine->appScriptObject().endBackgroundUpdate();
}

ScriptEngine &ScriptSession::engine()
{
    Q_ASSERT(!onMainThread());
    return *mScriptEngine;
}

void ScriptSession::applySessionUpdates()
{
    Q_ASSERT(onMainThread());
    mScriptEngine->appScriptObject()
        .applyBackgroundUpdates();
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

bool ScriptSession::usesViewportSize(const QString &fileName) const
{
    Q_ASSERT(onMainThread());
    return mScriptEngine->appScriptObject().usesViewportSize(fileName);
}

void ScriptSession::update()
{
    Q_ASSERT(onMainThread());
    mScriptEngine->appScriptObject().update();
}

MessageList ScriptSession::resetMessages()
{
    return mScriptEngine->resetMessages();
}
