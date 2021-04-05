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

    void updateVariables(MessagePtrSet &messages);
    ScriptVariable getVariable(const QString &variableName,
        const QStringList &valueExpressions,
        ItemId itemId, MessagePtrSet &messages);
    ScriptVariable getVariable(
        const QString &valueExpression,
        ItemId itemId, MessagePtrSet &messages);

    template<typename T>
    QJSValue toJsValue(const T &value) { return mJsEngine->toScriptValue(value); }

private:
    struct Variable {
        QString name;
        QStringList expressions;
        QWeakPointer<ScriptValueList> values;
    };

    QJSValue evaluate(const QString &program, const QString &fileName = QString(), int lineNumber = 1);
    bool updateVariable(const Variable &variable, ItemId itemId, MessagePtrSet &messages);

    QJSEngine *mJsEngine{ };
    QThread *mInterruptThread{ };
    QTimer *mInterruptTimer{ };
    QList<Variable> mVariables;
};

ScriptValue evaluateValueExpression(const QString &expression, bool *ok);
int evaluateIntExpression(const QString &expression, bool *ok);

#endif // SCRIPTENGINE_H
