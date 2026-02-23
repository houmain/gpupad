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
    Q_PROPERTY(int frameIndex READ frameIndex WRITE setFrameIndex)
    Q_PROPERTY(double frameRate READ frameRate WRITE setFrameRate)
    Q_PROPERTY(double time READ time WRITE setTime)
    Q_PROPERTY(double timeDelta READ timeDelta CONSTANT)
    Q_PROPERTY(QJSValue session READ session CONSTANT)
    Q_PROPERTY(QJSValue mouse READ mouse CONSTANT)
    Q_PROPERTY(QJSValue keyboard READ keyboard CONSTANT)

public:
    AppScriptObject(const ScriptEnginePtr &enginePtr, const QString &basePath);
    ~AppScriptObject();

    int frameIndex() const { return mFrameIndex; }
    void setFrameIndex(int index);
    double frameRate() const { return mFrameRate; }
    void setFrameRate(double frameRate);
    double time() const { return mTime; }
    void setTime(double time);
    double timeDelta() const { return mTime - mPrevTime; }
    QJSValue date() const { return mDate; }
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
    QJSEngine &jsEngine() { return *mJsEngine; }
    QString getAbsolutePath(const QString &fileName) const;
    void dispatchToMainThread(const std::function<void()> &function);

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
    QMap<QString, QJSValue> mLoadedLibraries;
    AppScriptObject_MainThreadCalls *mMainThreadCalls{};
    int mFrameIndex{};
    double mFrameRate{};
    double mTime{};
    double mPrevTime{};
    QJSValue mDate;
};
