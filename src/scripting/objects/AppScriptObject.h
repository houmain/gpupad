#pragma once

#include "MessageList.h"
#include <QObject>
#include <QJSValue>

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
    explicit AppScriptObject(QObject *parent = nullptr);
    explicit AppScriptObject(QJSEngine *engine);
    void initializeEngine(QJSEngine *engine);

    QJSValue session() { return mSessionProperty; }
    QJSValue mouse() { return mMouseProperty; }
    QJSValue keyboard() { return mKeyboardProperty; }

    Q_INVOKABLE QJSValue loadLibrary(const QString &fileName);
    Q_INVOKABLE QJSValue enumerateFiles(const QString &pattern);
    Q_INVOKABLE QJSValue writeTextFile(const QString &fileName,
        const QString &string);
    Q_INVOKABLE QJSValue readTextFile(const QString &fileName);

    void update();
    bool usesMouseState() const;
    bool usesKeyboardState() const;
    SessionScriptObject &sessionScriptObject() { return *mSessionScriptObject; }

private:
    QJSEngine &engine();

    QJSEngine *mEngine{};
    SessionScriptObject *mSessionScriptObject{};
    MouseScriptObject *mMouseScriptObject{};
    KeyboardScriptObject *mKeyboardScriptObject{};
    QJSValue mSessionProperty;
    QJSValue mMouseProperty;
    QJSValue mKeyboardProperty;
};
