#pragma once

#include "MessageList.h"
#include <QObject>
#include <vector>

using ScriptValue = double;
using ScriptValueList = QList<ScriptValue>;

class ScriptEngine : public QObject
{
public:
    virtual void setTimeout(int msec) = 0;
    virtual void setGlobal(const QString &name, QObject *object) = 0;
    virtual void setGlobal(const QString &name,
        const ScriptValueList &values) = 0;
    virtual void validateScript(const QString &script, const QString &fileName,
        MessagePtrSet &messages) = 0;
    virtual void evaluateScript(const QString &script, const QString &fileName,
        MessagePtrSet &messages) = 0;
    virtual ScriptValueList evaluateValues(const QString &valueExpressions,
        ItemId itemId, MessagePtrSet &messages) = 0;

    ScriptValueList evaluateValues(const QStringList &valueExpressions,
        ItemId itemId, MessagePtrSet &messages);
    ScriptValue evaluateValue(const QString &valueExpression, ItemId itemId,
        MessagePtrSet &messages);
    int evaluateInt(const QString &valueExpression, ItemId itemId,
        MessagePtrSet &messages);

protected:
    explicit ScriptEngine(QObject *parent = nullptr);
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
