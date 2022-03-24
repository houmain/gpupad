
#include "ScriptEngine.h"

ScriptEngine::ScriptEngine(QObject *parent)
  : QObject(parent)
{
}

ScriptValueList ScriptEngine::evaluateValues(const QStringList &valueExpressions,
    ItemId itemId, MessagePtrSet &messages)
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
    const auto values = evaluateValues(valueExpression, itemId, messages);
    return (values.isEmpty() ? 0.0 : values.first());
}

int ScriptEngine::evaluateInt(const QString &valueExpression,
      ItemId itemId, MessagePtrSet &messages)
{
    return static_cast<int>(evaluateValue(valueExpression, itemId, messages) + 0.5);
}
