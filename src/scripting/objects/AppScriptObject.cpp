
#include "AppScriptObject.h"
#include "SessionScriptObject.h"
#include "KeyboardScriptObject.h"
#include "MouseScriptObject.h"
#include "LibraryScriptObject.h"
#include "../ScriptEngine.h"
#include "../CustomActions.h"
#include "Singletons.h"
#include "FileCache.h"
#include "editors/EditorManager.h"
#include <QDirIterator>
#include <QJSEngine>
#include <QCoreApplication>

AppScriptObject::AppScriptObject(const ScriptEnginePtr &enginePtr,
    const QString &basePath)
    : QObject(static_cast<QObject *>(enginePtr.get()))
    , mEnginePtr(enginePtr)
    , mJsEngine(&enginePtr->jsEngine())
    , mBasePath(basePath)
    , mSessionScriptObject(new SessionScriptObject(this))
    , mMouseScriptObject(new MouseScriptObject(this))
    , mKeyboardScriptObject(new KeyboardScriptObject(this))
{
    mSessionScriptObject->initializeEngine(mJsEngine);
    mSessionProperty = mJsEngine->newQObject(mSessionScriptObject);
    mMouseProperty = mJsEngine->newQObject(mMouseScriptObject);
    mKeyboardProperty = mJsEngine->newQObject(mKeyboardScriptObject);
}

QString AppScriptObject::getAbsolutePath(const QString &fileName) const
{
    return toNativeCanonicalFilePath(mBasePath.filePath(fileName));
}

void AppScriptObject::setSelection(const QModelIndexList &selectedIndices)
{
    mSelectionProperty = mJsEngine->newArray(selectedIndices.size());
    auto i = 0;
    for (const auto &index : selectedIndices)
        mSelectionProperty.setProperty(i++,
            sessionScriptObject().getItem(index));
}

void AppScriptObject::update()
{
    mFrameIndex++;

    auto &inputState = Singletons::inputState();
    inputState.update();
    mMouseScriptObject->update(inputState);
    mKeyboardScriptObject->update(inputState);
}

bool AppScriptObject::usesMouseState() const
{
    return mMouseScriptObject->wasRead();
}

bool AppScriptObject::usesKeyboardState() const
{
    return mKeyboardScriptObject->wasRead();
}

QJSValue AppScriptObject::session()
{
    if (!mSessionScriptObject->available()) {
        mJsEngine->throwError(QJSValue::EvalError,
            "Session object not available");
        return QJSValue();
    }
    return mSessionProperty;
}

QJSValue AppScriptObject::openEditor(QString fileName)
{
    if (!onMainThread())
        return {};

    fileName = getAbsolutePath(fileName);
    auto &editorManager = Singletons::editorManager();
    if (fileName.endsWith(".qml", Qt::CaseInsensitive)) {
        return static_cast<bool>(
            editorManager.openQmlView(fileName, mEnginePtr.lock()));
    }
    return static_cast<bool>(editorManager.openEditor(fileName));
}

QJSValue AppScriptObject::loadLibrary(QString fileName)
{
    // search in script's base path and in libs
    auto searchPaths = QList<QDir>();
    if (mBasePath != QDir())
        searchPaths += mBasePath;
#if !defined(NDEBUG)
    searchPaths += QDir(
        QCoreApplication::applicationDirPath() + "/extra/actions/" + fileName);
#endif
    for (const auto &dir : getApplicationDirectories("actions"))
        searchPaths += dir.filePath(fileName);
    searchPaths += getApplicationDirectories("libs");

    auto engine = mEnginePtr.lock();
    if (FileDialog::getFileExtension(fileName) == "js") {
        for (const auto &dir : std::as_const(searchPaths))
            if (dir.exists(fileName)) {
                const auto filePath =
                    toNativeCanonicalFilePath(dir.filePath(fileName));
                auto source = QString();
                if (Singletons::fileCache().getSource(filePath, &source)) {
                    engine->evaluateScript(source, filePath, mMessages);
                    return {};
                }
            }
    } else {
        auto paths = QStringList();
        for (const auto &dir : std::as_const(searchPaths))
            paths += dir.path();
        auto library = std::make_unique<LibraryScriptObject>();
        if (library->load(&jsEngine(), fileName, paths))
            return jsEngine().newQObject(library.release());
    }
    jsEngine().throwError("Loading library '" + fileName + "' failed");
    return {};
}

QJSValue AppScriptObject::callAction(QString id, QJSValue arguments)
{
    auto engine = mEnginePtr.lock();
    engine->setGlobal("arguments", arguments);

    auto &customActions = Singletons::customActions();
    const auto applied =
        customActions.applyActionInEngine(id, *engine, mMessages);
    engine->setGlobal("arguments", QJSValue::UndefinedValue);
    if (!applied)
        jsEngine().throwError("Applying action '" + id + "' failed");

    const auto result = engine->getGlobal("result");
    engine->setGlobal("result", QJSValue::UndefinedValue);

    return result;
}

QJSValue AppScriptObject::callAction(QString id)
{
    return callAction(id, mJsEngine->newObject());
}

QJSValue AppScriptObject::openFileDialog(QString pattern)
{
    if (!onMainThread())
        return {};

    auto options = FileDialog::Options{};
    Singletons::fileDialog().setDirectory(mLastFileDialogDirectory);

    if (Singletons::fileDialog().exec(options, pattern)) {
        mLastFileDialogDirectory = Singletons::fileDialog().directory();
        return Singletons::fileDialog().fileName();
    }
    return {};
}

QJSValue AppScriptObject::enumerateFiles(QString pattern)
{
    auto dir = QDir(getAbsolutePath(pattern));
    dir.setFilter(QDir::Files);
    dir.setSorting(QDir::Name);
    auto it = QDirIterator(dir, QDirIterator::Subdirectories);
    auto result = jsEngine().newArray();
    for (auto i = 0; it.hasNext();)
        result.setProperty(i++, it.next());
    return result;
}

QJSValue AppScriptObject::writeTextFile(QString fileName, const QString &string)
{
    fileName = getAbsolutePath(fileName);
    auto file = QFile(fileName);
    if (!file.open(QFile::WriteOnly | QFile::Text))
        return false;
    file.write(string.toUtf8());
    file.close();

    Singletons::fileCache().invalidateFile(fileName);
    return true;
}

QJSValue AppScriptObject::readTextFile(QString fileName)
{
    auto source = QString{};
    if (!Singletons::fileCache().getSource(getAbsolutePath(fileName), &source))
        return QJSValue::UndefinedValue;
    return source;
}
