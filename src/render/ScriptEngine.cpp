#include "ScriptEngine.h"
#include "Singletons.h"
#include "FileCache.h"
#include <QJSEngine>

ScriptEngine::ScriptEngine()
    : mJsEngine(new QJSEngine())
{
    mJsEngine->installExtensions(QJSEngine::ConsoleExtension);
}

ScriptEngine::~ScriptEngine() = default;

void ScriptEngine::evalScript(
    const QString &fileName, ItemId itemId)
{
    auto source = QString();
    if (!Singletons::fileCache().getSource(fileName, &source)) {
        mMessages += Singletons::messageList().insert(
            itemId, MessageType::LoadingFileFailed, fileName);
        return;
    }

    auto result = mJsEngine->evaluate(source, fileName);
    if (result.isError())
        mMessages += Singletons::messageList().insert(
            fileName, result.property("lineNumber").toInt(),
            MessageType::Error, result.toString());
}

QStringList ScriptEngine::evalValue(
    const QStringList &fieldExpressions, ItemId itemId)
{
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
