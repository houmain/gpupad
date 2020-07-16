#include "ScriptEngine.h"
#include "Singletons.h"
#include "session/SessionModel.h"
#include "FileDialog.h"
#include <QMutex>

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
        (*gCurrentMessageList) += MessageList::insert(
            context.file, context.line, messageType, message, false);
    }

    template<typename F>
    void redirectConsoleMessages(MessagePtrSet &messages, F &&function)
    {
        QMutexLocker locker(&gMutex);
        gCurrentMessageList = &messages;
        auto prevMessageHandler = qInstallMessageHandler(consoleMessageHandler);
        function();
        qInstallMessageHandler(prevMessageHandler);
        gCurrentMessageList = nullptr;
    }
} // namespace

ScriptEngine::ScriptEngine()
    : mJsEngine(new QJSEngine())
{
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
}

ScriptEngine::~ScriptEngine() = default;

void ScriptEngine::setGlobal(const QString &name, QJSValue value)
{
    mJsEngine->globalObject().setProperty(name, value);
}

void ScriptEngine::setGlobal(const QString &name, QObject *object)
{
    mJsEngine->globalObject().setProperty(name, mJsEngine->newQObject(object));
}

QJSValue ScriptEngine::getGlobal(const QString &name)
{
    return mJsEngine->globalObject().property(name);
}

QJSValue ScriptEngine::call(QJSValue &callable, const QJSValueList &args,
    ItemId itemId, MessagePtrSet &messages)
{
    auto result = QJSValue();
    redirectConsoleMessages(messages, [&]() {
        result = callable.call(args);
        if (result.isError())
            messages += MessageList::insert(
                itemId, MessageType::ScriptError, result.toString());
    });
    return result;
}

void ScriptEngine::evaluateScript(const QString &script, const QString &fileName) 
{
    redirectConsoleMessages(mMessages, [&]() {
        auto result = mJsEngine->evaluate(script, fileName);
        if (result.isError())
            mMessages += MessageList::insert(
                fileName, result.property("lineNumber").toInt(),
                MessageType::ScriptError, result.toString());
    });
}

void ScriptEngine::evaluateExpression(const QString &script, const QString &resultName,
    ItemId itemId, MessagePtrSet &messages) 
{
    redirectConsoleMessages(mMessages, [&]() {
        auto result = mJsEngine->evaluate(script);
        if (result.isError()) {
            messages += MessageList::insert(
                itemId, MessageType::ScriptError, result.toString());
        }
        else {
            setGlobal(resultName, result);
        }
    });
}

QList<double> ScriptEngine::evaluateValues(const QStringList &valueExpressions,
    ItemId itemId, MessagePtrSet &messages)
{
    auto values = QList<double>();
    redirectConsoleMessages(messages, [&]() {
        for (QString valueExpression : valueExpressions) {

            // fast path, when script is a number
            auto ok = false;
            auto value = valueExpression.toDouble(&ok);
            if (ok) {
                values.append(value);
                continue;
            }

            auto result = mJsEngine->evaluate(valueExpression);
            if (result.isError())
                messages += MessageList::insert(
                    itemId, MessageType::ScriptError, result.toString());

            if (result.isObject()) {
                for (auto i = 0u; ; ++i) {
                    auto value = result.property(i);
                    if (value.isUndefined())
                        break;
                    values.append(value.toNumber());
                }
            }
            else {
                values.append(result.toNumber());
            }
        }
    });
    return values;
}

double ScriptEngine::evaluateValue(const QString &valueExpression,
    ItemId itemId, MessagePtrSet &messages) 
{
    const auto values = evaluateValues({ valueExpression },
        itemId, messages);
    return (values.isEmpty() ? 0.0 : values.first());
}

