#include "ScriptEngine.h"
#include "Singletons.h"
#include "session/SessionModel.h"
#include "FileDialog.h"
#include <QJSEngine>

namespace {
    QMutex gMutex;
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

    template<typename F>
    void redirectConsoleMessages(MessagePtrSet &messages, F&& function)
    {
        QMutexLocker locker(&gMutex);
        gCurrentMessageList = &messages;
        auto prevMessageHandler = qInstallMessageHandler(consoleMessageHandler);
        function();
        qInstallMessageHandler(prevMessageHandler);
        gCurrentMessageList = nullptr;
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
              "if (typeof arguments[i] === 'object')"
                "text += JSON.stringify(arguments[i], null, 2);"
              "else "
                "text += arguments[i];"
            "log(text);"
           "};"
        "})();");
    mMessages.clear();
}

void ScriptEngine::reset(QList<Script> scripts)
{
    if (scripts == mScripts)
        return;

    reset();

    redirectConsoleMessages(mMessages, [&]() {
        foreach (const Script &script, scripts) {
            auto result = mJsEngine->evaluate(script.source, script.fileName);
            if (result.isError())
                mMessages += Singletons::messageList().insert(
                    script.fileName, result.property("lineNumber").toInt(),
                    MessageType::ScriptError, result.toString());
        }
    });
    mScripts = scripts;
}

void ScriptEngine::setGlobal(const QString &name, QObject *object)
{
    if (!mJsEngine)
        reset();

    mJsEngine->globalObject().setProperty(name, mJsEngine->newQObject(object));
}

QVariant ScriptEngine::evaluate(const QString &expression,
    ItemId itemId, MessagePtrSet &messages)
{
    if (!mJsEngine)
        reset();

    auto result = QJSValue();
    redirectConsoleMessages(messages, [&]() {
        result = mJsEngine->evaluate(expression);
        if (result.isError())
            messages += Singletons::messageList().insert(
                itemId, MessageType::ScriptError, result.toString());
    });
    return result.toVariant();
}

QStringList ScriptEngine::evaluateValue(const QStringList &fieldExpressions,
    ItemId itemId, MessagePtrSet &messages)
{
    if (!mJsEngine)
        reset();

    auto fields = QStringList();
    redirectConsoleMessages(messages, [&]() {
        foreach (QString fieldExpression, fieldExpressions) {
            auto result = mJsEngine->evaluate(fieldExpression);
            if (result.isError())
                messages += Singletons::messageList().insert(
                    itemId, MessageType::ScriptError, result.toString());

            if (result.isObject()) {
                for (auto i = 0u; ; ++i) {
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
    });
    return fields;
}
