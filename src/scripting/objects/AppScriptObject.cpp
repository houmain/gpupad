
#include "AppScriptObject.h"
#include "SessionScriptObject.h"
#include "KeyboardScriptObject.h"
#include "MouseScriptObject.h"
#include "Singletons.h"
#include <QJSEngine>

AppScriptObject::AppScriptObject(QObject *parent)
    : QObject(parent)
    , mSessionScriptObject(new SessionScriptObject(this))
    , mMouseScriptObject(new MouseScriptObject(this))
    , mKeyboardScriptObject(new KeyboardScriptObject(this))
{
}

AppScriptObject::AppScriptObject(QJSEngine *engine)
    : AppScriptObject(static_cast<QObject *>(engine))
{
    initializeEngine(engine);
}

void AppScriptObject::initializeEngine(QJSEngine *engine)
{
    mEngine = engine;
    mSessionScriptObject->initializeEngine(engine);
    mSessionProperty = engine->newQObject(mSessionScriptObject);
    mMouseProperty = engine->newQObject(mMouseScriptObject);
    mKeyboardProperty = engine->newQObject(mKeyboardScriptObject);
}

QJSEngine &AppScriptObject::engine()
{
    Q_ASSERT(mEngine);
    return *mEngine;
}

void AppScriptObject::update()
{
    Singletons::inputState().update();
    mMouseScriptObject->update(Singletons::inputState());
    mKeyboardScriptObject->update(Singletons::inputState());
}

bool AppScriptObject::usesMouseState() const
{
    return mMouseScriptObject->wasRead();
}

bool AppScriptObject::usesKeyboardState() const
{
    return mKeyboardScriptObject->wasRead();
}
