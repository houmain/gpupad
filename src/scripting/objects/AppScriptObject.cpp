
#include "AppScriptObject.h"
#include "SessionScriptObject.h"
#include "KeyboardScriptObject.h"
#include "MouseScriptObject.h"
#include "LibraryScriptObject.h"
#include "Singletons.h"
#include "FileCache.h"
#include <QDirIterator>
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

QJSValue AppScriptObject::loadLibrary(const QString &fileName)
{
    auto library = std::make_unique<LibraryScriptObject>();
    if (!library->load(mEngine, fileName))
        return {};
    return engine().newQObject(library.release());
}

QJSValue AppScriptObject::enumerateFiles(const QString &pattern)
{
    auto dir = QFileInfo(pattern).dir();
    dir.setFilter(QDir::Files);
    dir.setSorting(QDir::Name);
    auto it = QDirIterator(dir, QDirIterator::Subdirectories);
    auto result = engine().newArray();
    for (auto i = 0; it.hasNext();)
        result.setProperty(i++, it.next());
    return result;
}

QJSValue AppScriptObject::writeTextFile(const QString &fileName,
    const QString &string)
{
    auto file = QFile(fileName);
    if (!file.open(QFile::WriteOnly | QFile::Text))
        return false;
    file.write(string.toUtf8());
    file.close();

    Singletons::fileCache().invalidateFile(fileName);
    return true;
}

QJSValue AppScriptObject::readTextFile(const QString &fileName)
{
    auto source = QString{};
    if (!Singletons::fileCache().getSource(fileName, &source))
        return QJSValue::UndefinedValue;
    return source;
}
