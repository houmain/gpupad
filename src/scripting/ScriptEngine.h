#ifndef SCRIPTENGINE_H
#define SCRIPTENGINE_H

#include "MessageList.h"
#include <QJSEngine>
#include <QJSValue>

using ScriptValue = double;
using ScriptValueList = QList<ScriptValue>;
class QTimer;

class ScriptVariable
{
public:
    int count() const;
    ScriptValue get() const;
    ScriptValue get(int index) const;

private:
    friend class ScriptEngine;
    QSharedPointer<ScriptValueList> mValues;
};

class ScriptEngine : public QObject
{
    Q_OBJECT
public:
    explicit ScriptEngine(QObject *parent = nullptr);
    ~ScriptEngine();

    void setGlobal(const QString &name, QJSValue value);
    void setGlobal(const QString &name, QObject *object);
    QJSValue getGlobal(const QString &name);
    QJSValue call(QJSValue &callable, const QJSValueList &args,
        ItemId itemId, MessagePtrSet &messages);
    void evaluateScript(const QString &script, const QString &fileName);
    void evaluateExpression(const QString &script, const QString &resultName, 
        ItemId itemId, MessagePtrSet &messages);
    ScriptValueList evaluateValues(const QStringList &valueExpressions,
        ItemId itemId, MessagePtrSet &messages);
    ScriptValue evaluateValue(const QString &valueExpression,
      ItemId itemId, MessagePtrSet &messages);
    int evaluateInt(const QString &valueExpression,
      ItemId itemId, MessagePtrSet &messages);

    void updateVariables();
    ScriptVariable getVariable(const QStringList &valueExpressions,
        ItemId itemId, MessagePtrSet &messages);
    ScriptVariable getVariable(const QString &valueExpression,
        ItemId itemId, MessagePtrSet &messages);

    template<typename T>
    QJSValue toJsValue(const T &value) { return mJsEngine->toScriptValue(value); }

private:
    QJSValue evaluate(const QString &program, const QString &fileName = QString(), int lineNumber = 1);

    QJSEngine *mJsEngine{ };
    QThread *mInterruptThread{ };
    QTimer *mInterruptTimer{ };
    MessagePtrSet mMessages;
    QList<QPair<QStringList, QWeakPointer<ScriptValueList>>> mVariables;
};

#endif // SCRIPTENGINE_H
