#pragma once

#include "MessageList.h"
#include <QObject>
#include <QJSValue>
#include <QModelIndex>
#include <QDir>

class SessionScriptObject;
class MouseScriptObject;
class KeyboardScriptObject;
using ScriptEnginePtr = std::shared_ptr<class ScriptEngine>;
using WeakScriptEnginePtr = std::weak_ptr<class ScriptEngine>;

class AppScriptObject final : public QObject
{
    Q_OBJECT
    Q_PROPERTY(int frameIndex READ frameIndex CONSTANT)
    Q_PROPERTY(QJSValue session READ session CONSTANT)
    Q_PROPERTY(QJSValue mouse READ mouse CONSTANT)
    Q_PROPERTY(QJSValue keyboard READ keyboard CONSTANT)

public:
    AppScriptObject(const ScriptEnginePtr &enginePtr, const QString &basePath);

    int frameIndex() const { return mFrameIndex; }
    QJSValue session();
    QJSValue mouse() { return mMouseProperty; }
    QJSValue keyboard() { return mKeyboardProperty; }

    Q_INVOKABLE QJSValue openEditor(QString fileName);
    Q_INVOKABLE QJSValue loadLibrary(QString fileName);
    Q_INVOKABLE QJSValue callAction(QString id);
    Q_INVOKABLE QJSValue callAction(QString id, QJSValue arguments);
    Q_INVOKABLE QJSValue openFileDialog(QString pattern);
    Q_INVOKABLE QJSValue enumerateFiles(QString pattern);
    Q_INVOKABLE QJSValue writeTextFile(QString fileName, const QString &string);
    Q_INVOKABLE QJSValue readTextFile(QString fileName);

    void update();
    bool usesMouseState() const;
    bool usesKeyboardState() const;
    SessionScriptObject &sessionScriptObject() { return *mSessionScriptObject; }

private:
    QJSEngine &jsEngine() { return *mJsEngine; }
    QString getAbsolutePath(const QString &fileName) const;

    WeakScriptEnginePtr mEnginePtr;
    QJSEngine *mJsEngine{};
    QDir mBasePath;
    SessionScriptObject *mSessionScriptObject{};
    MouseScriptObject *mMouseScriptObject{};
    KeyboardScriptObject *mKeyboardScriptObject{};
    QJSValue mSessionProperty;
    QJSValue mMouseProperty;
    QJSValue mKeyboardProperty;
    QDir mLastFileDialogDirectory;
    int mFrameIndex{ };
};
