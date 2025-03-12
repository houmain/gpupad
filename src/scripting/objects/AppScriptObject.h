#pragma once

#include "MessageList.h"
#include <QObject>
#include <QJSValue>
#include <QDir>

class SessionScriptObject;
class MouseScriptObject;
class KeyboardScriptObject;

class AppScriptObject final : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QJSValue session READ session CONSTANT)
    Q_PROPERTY(QJSValue mouse READ mouse CONSTANT)
    Q_PROPERTY(QJSValue keyboard READ keyboard CONSTANT)

public:
    explicit AppScriptObject(const QString &scriptPath,
        QObject *parent = nullptr);
    AppScriptObject(const QString &scriptPath, QJSEngine *engine);
    void initializeEngine(QJSEngine *engine);

    QJSValue session() { return mSessionProperty; }
    QJSValue mouse() { return mMouseProperty; }
    QJSValue keyboard() { return mKeyboardProperty; }

    Q_INVOKABLE QJSValue openEditor(QString fileName);
    Q_INVOKABLE QJSValue loadLibrary(QString fileName);
    Q_INVOKABLE QJSValue enumerateFiles(QString pattern);
    Q_INVOKABLE QJSValue writeTextFile(QString fileName,
        const QString &string);
    Q_INVOKABLE QJSValue readTextFile(QString fileName);

    void update();
    bool usesMouseState() const;
    bool usesKeyboardState() const;
    SessionScriptObject &sessionScriptObject() { return *mSessionScriptObject; }

private:
    QJSEngine &engine();
    QString getAbsolutePath(const QString &fileName) const;

    QDir mBasePath;
    QJSEngine *mEngine{};
    SessionScriptObject *mSessionScriptObject{};
    MouseScriptObject *mMouseScriptObject{};
    KeyboardScriptObject *mKeyboardScriptObject{};
    QJSValue mSessionProperty;
    QJSValue mMouseProperty;
    QJSValue mKeyboardProperty;
};
