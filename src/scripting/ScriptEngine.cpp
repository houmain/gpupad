
#include "ScriptEngine.h"

ScriptEngine::ScriptEngine(QObject *parent) : QObject(parent) { }

ScriptValueList ScriptEngine::evaluateValues(
    const QStringList &valueExpressions, ItemId itemId, MessagePtrSet &messages)
{
    auto values = QList<double>();
    for (const QString &valueExpression : valueExpressions) {

        // fast path, when expression is empty or a number
        if (valueExpression.isEmpty()) {
            values.append(0.0);
            continue;
        }
        auto ok = false;
        auto value = valueExpression.toDouble(&ok);
        if (ok) {
            values.append(value);
            continue;
        }
        values += evaluateValues(valueExpression, itemId, messages);
    }
    return values;
}

ScriptValue ScriptEngine::evaluateValue(const QString &valueExpression,
    ItemId itemId, MessagePtrSet &messages)
{
    // fast path, when expression is empty or a number
    if (valueExpression.isEmpty())
        return 0.0;
    auto ok = false;
    auto value = valueExpression.toDouble(&ok);
    if (ok)
        return value;

    const auto values = evaluateValues(valueExpression, itemId, messages);
    return (values.isEmpty() ? 0.0 : values.first());
}

int ScriptEngine::evaluateInt(const QString &valueExpression, ItemId itemId,
    MessagePtrSet &messages)
{
    const auto value = evaluateValue(valueExpression, itemId, messages);
    Q_ASSERT(std::isfinite(value));
    if (!std::isfinite(value))
        return 0;
    return static_cast<int>(value + 0.5);
}

void checkValueCount(const ScriptValueList &values, int count, int offset,
    ItemId itemId, MessagePtrSet &messages)
{
    if (offset < 0) {
        // check that value counts match (allow setting 4 components of vec3)
        if (values.count() != count && (count != 3 || values.count() != 4))
            messages += MessageList::insert(itemId,
                MessageType::UniformComponentMismatch,
                QString("(%1/%2)").arg(values.count()).arg(count));
    } else {
        // check that there are enough values
        if (values.count() < offset + count)
            messages += MessageList::insert(itemId,
                MessageType::UniformComponentMismatch,
                QString("(%1 < %2)").arg(values.count()).arg(offset + count));
    }
}
