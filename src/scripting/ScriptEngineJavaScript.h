#pragma once

#include "ScriptEngine.h"
#include <QJSEngine>
#include <QJSValue>

class QTimer;
class ScriptConsole;

class ScriptEngineJavaScript : public ScriptEngine
{
    Q_OBJECT
public:
    explicit ScriptEngineJavaScript(QObject *parent = nullptr);
    ~ScriptEngineJavaScript();

    void setOmitReferenceErrors();
    void setTimeout(int msec) override;
    void setGlobal(const QString &name, QObject *object) override;
    void setGlobal(const QString &name, const ScriptValueList &values) override;
    void validateScript(const QString &script, const QString &fileName,
        MessagePtrSet &messages) override;
    void evaluateScript(const QString &script, const QString &fileName,
        MessagePtrSet &messages) override;
    ScriptValueList evaluateValues(const QString &valueExpression,
        ItemId itemId, MessagePtrSet &messages) override;

    QJSEngine *jsEngine() { return mJsEngine; }
    void setGlobal(const QString &name, QJSValue value);
    QJSValue getGlobal(const QString &name);
    QJSValue call(QJSValue &callable, const QJSValueList &args, ItemId itemId,
        MessagePtrSet &messages);

    template <typename T>
    QJSValue toJsValue(const T &value)
    {
        return mJsEngine->toScriptValue(value);
    }

private:
    QJSValue evaluate(const QString &program,
        const QString &fileName = QString(), int lineNumber = 1);
    void outputError(const QJSValue &result, ItemId itemId,
        MessagePtrSet &messages);

    const QThread &mOnThread;
    QJSEngine *mJsEngine{};
    ScriptConsole *mConsole{};
    QThread *mInterruptThread{};
    QTimer *mInterruptTimer{};
    bool mOmitReferenceErrors{};
};