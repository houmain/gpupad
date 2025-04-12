#pragma once

#include "MessageList.h"
#include <QObject>
#include <QJSEngine>
#include <QJSValue>
#include <vector>

using ScriptEnginePtr = std::shared_ptr<class ScriptEngine>;
using ScriptValue = double;
using ScriptValueList = QList<ScriptValue>;
class ConsoleScriptObject;
class AppScriptObject;
class QTimer;

class ScriptEngine : public QObject
{
public:
    static ScriptEnginePtr make(const QString &basePath = "",
        QThread *thread = nullptr, QObject *parent = nullptr);
    ~ScriptEngine();

    void setOmitReferenceErrors();
    void setTimeout(int msec);
    void setGlobal(const QString &name, QObject *object);
    void setGlobal(const QString &name, const ScriptValueList &values);
    void validateScript(const QString &script, const QString &fileName,
        MessagePtrSet &messages);
    void evaluateScript(const QString &script, const QString &fileName,
        MessagePtrSet &messages);
    ScriptValueList evaluateValues(const QString &valueExpression,
        ItemId itemId, MessagePtrSet &messages);
    ScriptValueList evaluateValues(const QStringList &valueExpressions,
        ItemId itemId, MessagePtrSet &messages);
    ScriptValue evaluateValue(const QString &valueExpression, ItemId itemId,
        MessagePtrSet &messages);
    int evaluateInt(const QString &valueExpression, ItemId itemId,
        MessagePtrSet &messages);
    QJSEngine &jsEngine();

    AppScriptObject &appScriptObject() { return *mAppScriptObject; }

    void setGlobal(const QString &name, QJSValue value);
    QJSValue getGlobal(const QString &name);
    QJSValue call(QJSValue &callable, const QJSValueList &args, ItemId itemId,
        MessagePtrSet &messages);

    template <typename T>
    QJSValue toJsValue(const T &value)
    {
        return jsEngine().toScriptValue(value);
    }

private:
    ScriptEngine(QObject* parent);
    void initialize(const ScriptEnginePtr &self, const QString &basePath);
    void resetInterruptTimer();
    void outputError(const QJSValue &result, ItemId itemId,
        MessagePtrSet &messages);

    QJSEngine *mJsEngine{};
    ConsoleScriptObject *mConsoleScriptObject{};
    AppScriptObject *mAppScriptObject{};
    QThread *mInterruptThread{};
    QTimer *mInterruptTimer{};
    bool mOmitReferenceErrors{};
};

void checkValueCount(int valueCount, int offset, int count, ItemId itemId,
    MessagePtrSet &messages);

template <typename T>
std::vector<T> getValues(ScriptEngine &scriptEngine,
    const QStringList &expressions, int offset, int count, ItemId itemId,
    MessagePtrSet &messages)
{
    const auto values =
        scriptEngine.evaluateValues(expressions, itemId, messages);

    checkValueCount(values.size(), offset, count, itemId, messages);

    auto results = std::vector<T>();
    results.reserve(count);
    for (const auto &value : values)
        if (offset-- <= 0)
            results.push_back(static_cast<T>(value));
    results.resize(count);
    return results;
}
