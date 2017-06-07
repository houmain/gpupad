#include "ScriptEngine.h"

void ScriptEngine::evalScript(const QString &fileName,
    const QString &source)
{
    mScripts.push_back({ fileName, source });
    mJsEngine.reset();
}

QStringList ScriptEngine::evalValue(const QStringList &fieldExpressions,
    MessageList &messages)
{
    evalScripts(messages);

    auto fields = QStringList();
    foreach (QString fieldExpression, fieldExpressions) {
        auto result = mJsEngine->evaluate(fieldExpression);
        if (result.isError())
            messages.insert(MessageType::Error, result.toString());

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

void ScriptEngine::evalScripts(MessageList &messages)
{
    if (!mJsEngine) {
        mJsEngine = std::make_unique<QJSEngine>();
        mJsEngine->installExtensions(QJSEngine::ConsoleExtension);
        for (Script &script : mScripts) {
            auto result = mJsEngine->evaluate(script.source, script.fileName);
            if (result.isError()) {
                messages.setContext(script.fileName);
                messages.insert(MessageType::Error, result.toString(),
                    result.property("lineNumber").toInt());
            }
        }
    }
}
