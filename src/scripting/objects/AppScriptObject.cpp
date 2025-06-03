
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
#include "editors/qml/QmlView.h"
#include <QApplication>
#include <QDirIterator>
#include <QJSEngine>
#include <QCoreApplication>
#include <atomic>

namespace {
    std::atomic<int> gLastEditorWidth{ 2 };
    std::atomic<int> gLastEditorHeight{ 2 };
    std::atomic<int> gLastMousePositionX{ 1 };
    std::atomic<int> gLastMousePositionY{ 1 };
} // namespace

bool AppScriptObject_MainThreadCalls::openQmlView(QString fileName,
    QString title, ScriptEnginePtr enginePtr)
{
    const auto editor =
        Singletons::editorManager().openQmlView(fileName, enginePtr);
    if (!editor)
        return false;

    // set cleaned up title
    title = title.remove("&").replace("...", "");
    if (!title.isEmpty())
        editor->setWindowTitle(title);

    return true;
}

bool AppScriptObject_MainThreadCalls::openEditor(QString fileName)
{
    return static_cast<bool>(Singletons::editorManager().openEditor(fileName));
}

QString AppScriptObject_MainThreadCalls::openFileDialog(QString pattern)
{
    auto options = FileDialog::Options{};
    Singletons::fileDialog().setDirectory(mLastFileDialogDirectory);

    if (!Singletons::fileDialog().exec(options, pattern))
        return {};
    mLastFileDialogDirectory = Singletons::fileDialog().directory();
    return Singletons::fileDialog().fileName();
}

//-------------------------------------------------------------------------

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

    mMainThreadCalls = new AppScriptObject_MainThreadCalls();
    if (!onMainThread())
        mMainThreadCalls->moveToThread(QApplication::instance()->thread());

    auto inputState = InputState();
    inputState.setEditorSize({
        gLastEditorWidth.load(),
        gLastEditorHeight.load(),
    });
    inputState.setMousePosition({
        gLastMousePositionX.load(),
        gLastMousePositionY.load(),
    });
    inputState.update();
    mMouseScriptObject->update(inputState);
    mKeyboardScriptObject->update(inputState);
}

AppScriptObject::~AppScriptObject()
{
    mMainThreadCalls->deleteLater();
}

QString AppScriptObject::getAbsolutePath(const QString &fileName) const
{
    if (FileDialog::isEmptyOrUntitled(fileName))
        return fileName;
    return toNativeCanonicalFilePath(mBasePath.filePath(fileName));
}

void AppScriptObject::update()
{
    Q_ASSERT(onMainThread());

    ++mFrameIndex;

    auto &inputState = Singletons::inputState();
    inputState.update();
    gLastEditorWidth.store(inputState.editorSize().width());
    gLastEditorHeight.store(inputState.editorSize().height());
    gLastMousePositionX.store(inputState.mousePosition().x());
    gLastMousePositionY.store(inputState.mousePosition().y());

    mMouseScriptObject->update(inputState);
    mKeyboardScriptObject->update(inputState);
}

bool AppScriptObject::usesMouseState() const
{
    Q_ASSERT(onMainThread());
    return mMouseScriptObject->wasRead();
}

bool AppScriptObject::usesKeyboardState() const
{
    Q_ASSERT(onMainThread());
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

QJSValue AppScriptObject::openEditor(QString fileName, QString title)
{
    fileName = getAbsolutePath(fileName);
    auto result = false;
    dispatchToMainThread([&]() {
        if (fileName.endsWith(".qml", Qt::CaseInsensitive)) {
            result = static_cast<bool>(mMainThreadCalls->openQmlView(fileName,
                title, (onMainThread() ? mEnginePtr.lock() : nullptr)));
        } else {
            result = static_cast<bool>(mMainThreadCalls->openEditor(fileName));
        }
    });
    return result;
}

QJSValue AppScriptObject::loadLibrary(QString fileName)
{
    // only load each library once
    if (auto it = mLoadedLibraries.find(fileName); it != mLoadedLibraries.end())
        return *it;

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
                    engine->evaluateScript(source, filePath);
                    // script does not return anything for now
                    const auto libraryObject = jsEngine().newObject();
                    mLoadedLibraries[fileName] = libraryObject;
                    return libraryObject;
                }
            }
    } else {
        auto paths = QStringList();
        for (const auto &dir : std::as_const(searchPaths))
            paths += dir.path();
        auto library = std::make_unique<LibraryScriptObject>();
        if (library->load(&jsEngine(), fileName, paths)) {
            const auto libraryObject = jsEngine().newQObject(library.release());
            mLoadedLibraries[fileName] = libraryObject;
            return libraryObject;
        }
    }
    jsEngine().throwError("Loading library '" + fileName + "' failed");
    return {};
}

QJSValue AppScriptObject::callAction(QString id, QJSValue arguments)
{
    auto engine = mEnginePtr.lock();
    engine->setGlobal("arguments", arguments);

    auto &customActions = Singletons::customActions();
    const auto applied = customActions.applyActionInEngine(id, *engine);
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
    auto result = QString();
    dispatchToMainThread(
        [&]() { result = mMainThreadCalls->openFileDialog(pattern); });
    if (!result.isEmpty())
        return result;
    return {};
}

QJSValue AppScriptObject::enumerateFiles(QString pattern)
{
    const auto fileInfo = QFileInfo(getAbsolutePath(pattern));
    auto dir = fileInfo.dir();
    auto flags = QDirIterator::IteratorFlags{ QDirIterator::FollowSymlinks };
    if (dir.dirName() == "**") {
        dir.cdUp();
        flags |= QDirIterator::Subdirectories;
    }
    dir.setFilter(QDir::Files);
    dir.setNameFilters({ fileInfo.fileName() });
    dir.setSorting(QDir::Name);
    auto it = QDirIterator(dir, flags);
    auto result = jsEngine().newArray();
    for (auto i = 0; it.hasNext();)
        result.setProperty(i++, it.next());
    return result;
}

QJSValue AppScriptObject::writeTextFile(QString fileName, QString string)
{
    if (FileDialog::isEmptyOrUntitled(fileName))
        return false;

    fileName = getAbsolutePath(fileName);
    auto file = QFile(fileName);
    if (!file.open(QFile::WriteOnly | QFile::Text))
        return false;
    file.write(string.toUtf8());
    file.close();

    Singletons::fileCache().invalidateFile(fileName);
    return true;
}

QJSValue AppScriptObject::writeBinaryFile(QString fileName, QByteArray binary)
{
    if (FileDialog::isEmptyOrUntitled(fileName))
        return false;
    if (binary.isNull())
        return false;

    fileName = getAbsolutePath(fileName);
    auto file = QFile(fileName);
    if (!file.open(QFile::WriteOnly))
        return false;
    file.write(binary);
    file.close();

    Singletons::fileCache().invalidateFile(fileName);
    return true;
}

QJSValue AppScriptObject::readTextFile(QString fileName)
{
    if (FileDialog::isEmptyOrUntitled(fileName))
        return {};

    auto source = QString{};
    if (!Singletons::fileCache().getSource(getAbsolutePath(fileName), &source))
        return QJSValue::UndefinedValue;
    return source;
}
