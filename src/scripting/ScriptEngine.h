#ifndef SCRIPTENGINE_H
#define SCRIPTENGINE_H

#include "MessageList.h"
#include <QJSEngine>
#include <QJSValue>

class QTimer;
class ScriptConsole;

using ScriptValue = double;
using ScriptValueList = QList<ScriptValue>;

class ScriptEngine : public QObject
{
    Q_OBJECT
public:
    explicit ScriptEngine(QObject *parent = nullptr);
    ~ScriptEngine();

    void setTimeout(int msec);

    void setGlobal(const QString &name, QJSValue value);
    void setGlobal(const QString &name, QObject *object);
    void setGlobal(const QString &name, const ScriptValueList &values);
    QJSValue getGlobal(const QString &name);
    QJSValue call(QJSValue &callable, const QJSValueList &args,
        ItemId itemId, MessagePtrSet &messages);
    void evaluateScript(const QString &script,
        const QString &fileName, MessagePtrSet &messages);
    ScriptValueList evaluateValues(const QStringList &valueExpressions,
        ItemId itemId, MessagePtrSet &messages);
    ScriptValue evaluateValue(const QString &valueExpression,
        ItemId itemId, MessagePtrSet &messages);
    int evaluateInt(const QString &valueExpression,
        ItemId itemId, MessagePtrSet &messages);

    template<typename T>
    QJSValue toJsValue(const T &value) { return mJsEngine->toScriptValue(value); }

private:
    QJSValue evaluate(const QString &program, const QString &fileName = QString(), int lineNumber = 1);

    const QThread& mOnThread;
    QJSEngine *mJsEngine{ };
    ScriptConsole *mConsole{ };
    QThread *mInterruptThread{ };
    QTimer *mInterruptTimer{ };
};

#endif // SCRIPTENGINE_H
