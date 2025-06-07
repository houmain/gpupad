#pragma once

#include "MessageList.h"
#include <QObject>
#include <QJSValue>
#include <QModelIndex>
#include <QThread>
#include <QDir>

class SessionScriptObject;
class MouseScriptObject;
class KeyboardScriptObject;
class EditorScriptObject;
using ScriptEnginePtr = std::shared_ptr<class ScriptEngine>;
using WeakScriptEnginePtr = std::weak_ptr<class ScriptEngine>;

class AppScriptObject_MainThreadCalls : public QObject
{
    Q_OBJECT

public:
    bool openQmlView(QString fileName, QString title,
        ScriptEnginePtr enginePtr);
    bool openEditor(QString fileName);
    QString openFileDialog(QString pattern);

private:
    QDir mLastFileDialogDirectory;
};

class AppScriptObject final : public QObject
{
    Q_OBJECT
    Q_PROPERTY(int frameIndex READ frameIndex CONSTANT)
    Q_PROPERTY(QJSValue session READ session CONSTANT)
    Q_PROPERTY(QJSValue mouse READ mouse CONSTANT)
    Q_PROPERTY(QJSValue keyboard READ keyboard CONSTANT)

public:
    AppScriptObject(const ScriptEnginePtr &enginePtr, const QString &basePath);
    ~AppScriptObject();

    int frameIndex() const { return mFrameIndex; }
    QJSValue session();
    QJSValue mouse() { return mMouseProperty; }
    QJSValue keyboard() { return mKeyboardProperty; }

    Q_INVOKABLE QJSValue openEditor(QString fileName, QString title = {});
    Q_INVOKABLE QJSValue openFileDialog(QString pattern);
    Q_INVOKABLE QJSValue loadLibrary(QString fileName);
    Q_INVOKABLE QJSValue callAction(QString id);
    Q_INVOKABLE QJSValue callAction(QString id, QJSValue arguments);
    Q_INVOKABLE QJSValue enumerateFiles(QString pattern);
    Q_INVOKABLE QJSValue writeTextFile(QString fileName, QString string);
    Q_INVOKABLE QJSValue writeBinaryFile(QString fileName, QByteArray binary);
    Q_INVOKABLE QJSValue readTextFile(QString fileName);

    void deregisterEditorScriptObject(EditorScriptObject *object);
    void update();
    bool usesMouseState() const;
    bool usesKeyboardState() const;
    bool usesViewportSize(const QString &fileName) const;
    SessionScriptObject &sessionScriptObject() { return *mSessionScriptObject; }

private:
    template <typename F>
    void dispatchToMainThread(F &&function)
    {
        if (QThread::currentThread() != mMainThreadCalls->thread()) {
            QMetaObject::invokeMethod(mMainThreadCalls,
                std::forward<F>(function), Qt::BlockingQueuedConnection);
        } else {
            function();
        }
    }

    QJSEngine &jsEngine() { return *mJsEngine; }
    QString getAbsolutePath(const QString &fileName) const;

    WeakScriptEnginePtr mEnginePtr;
    QJSEngine *mJsEngine{};
    QDir mBasePath;
    SessionScriptObject *mSessionScriptObject{};
    MouseScriptObject *mMouseScriptObject{};
    KeyboardScriptObject *mKeyboardScriptObject{};
    std::map<EditorScriptObject *, QJSValue> mEditorScriptObjects;
    QJSValue mSessionProperty;
    QJSValue mMouseProperty;
    QJSValue mKeyboardProperty;
    int mFrameIndex{};
    QMap<QString, QJSValue> mLoadedLibraries;
    AppScriptObject_MainThreadCalls *mMainThreadCalls{};
};
