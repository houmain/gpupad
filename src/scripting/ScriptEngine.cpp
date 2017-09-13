#include "ScriptEngine.h"
#include "Singletons.h"
#include "session/SessionModel.h"
#include "FileDialog.h"
#include <QJSEngine>

namespace {
    MessagePtrSet* gCurrentMessageList;

    void consoleMessageHandler(QtMsgType type,
        const QMessageLogContext &context, const QString &msg)
    {
        auto fileName = FileDialog::getFileTitle(context.file);
        auto message = QString(msg).replace(context.file, fileName);
        auto messageType = (type == QtDebugMsg || type == QtInfoMsg ?
            MessageType::ScriptMessage : MessageType::ScriptError);
        (*gCurrentMessageList) += Singletons::messageList().insert(
            context.file, context.line, messageType, message, false);
    }
} // namespace

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
    mJsEngine->evaluate(
        "(function() {"
          "var log = console.log;"
          "console.log = function() {"
            "var text = '';"
            "for (var i = 0, n = arguments.length; i < n; i++)"
              "text += JSON.stringify(arguments[i], null, 2);"
            "log(text);"
           "};"
        "})();");
    mMessages.clear();
}

void ScriptEngine::evalScripts(QList<Script> scripts)
{
    if (scripts == mScripts)
        return;

    reset();

    gCurrentMessageList = &mMessages;
    auto prevMessageHandler = qInstallMessageHandler(consoleMessageHandler);

    foreach (const Script &script, scripts) {
        auto result = mJsEngine->evaluate(script.source, script.fileName);
        if (result.isError())
            mMessages += Singletons::messageList().insert(
                script.fileName, result.property("lineNumber").toInt(),
                MessageType::ScriptError, result.toString());
    }
    mScripts = scripts;

    qInstallMessageHandler(prevMessageHandler);
}

QStringList ScriptEngine::evalValue(const QStringList &fieldExpressions,
    ItemId itemId, MessagePtrSet &messages)
{
    if (!mJsEngine)
        reset();

    auto fields = QStringList();
    foreach (QString fieldExpression, fieldExpressions) {
        auto result = mJsEngine->evaluate(fieldExpression);
        if (result.isError())
            messages += Singletons::messageList().insert(
                itemId, MessageType::ScriptError, result.toString());

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
