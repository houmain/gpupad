#include "ScriptEngine.h"
#include "Singletons.h"
#include <QJSEngine>

bool operator==(const ScriptEngine::Script &a,
                const ScriptEngine::Script &b)
{
    return (a.fileName == b.fileName &&
            a.source == b.source);
}

ScriptEngine::ScriptEngine() = default;

ScriptEngine::~ScriptEngine() = default;

void ScriptEngine::reset()
{
    mJsEngine.reset(new QJSEngine());
    mJsEngine->installExtensions(QJSEngine::ConsoleExtension);
    mMessages.clear();
}

void ScriptEngine::evalScripts(QList<Script> scripts)
{
    if (scripts == mScripts)
        return;

    reset();

    foreach (const Script &script, scripts) {
        auto result = mJsEngine->evaluate(script.source, script.fileName);
        if (result.isError())
            mMessages += Singletons::messageList().insert(
                script.fileName, result.property("lineNumber").toInt(),
                MessageType::Error, result.toString());
    }
    mScripts = scripts;
}

QStringList ScriptEngine::evalValue(
    const QStringList &fieldExpressions, ItemId itemId)
{
    if (!mJsEngine)
        reset();

    auto fields = QStringList();
    foreach (QString fieldExpression, fieldExpressions) {
        auto result = mJsEngine->evaluate(fieldExpression);
        if (result.isError())
            mMessages += Singletons::messageList().insert(
                itemId, MessageType::Error, result.toString());

        if (result.isObject()) {
            for (auto i = 0; ; ++i) {
                auto field = result.property(i);
                if (field.isUndefined())
                    break;
                fields.append(field.toString());
            }
        }
        else {
            fields.append(result.toString());
        }
    }
    return fields;
}
