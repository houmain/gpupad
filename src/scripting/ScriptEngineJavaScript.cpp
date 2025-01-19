#include "ScriptEngineJavaScript.h"
#include "FileDialog.h"
#include "ScriptConsole.h"
#include "Singletons.h"
#include "session/SessionModel.h"
#include <QTextStream>
#include <QThread>
#include <QTimer>

namespace {
    void getFlattenedValuesRec(const QJSValue &value, ScriptValueList *values)
    {
        if (value.isObject() || value.isArray()) {
            for (auto i = 0u;; ++i) {
                auto element = value.property(i);
                if (element.isUndefined())
                    break;
                getFlattenedValuesRec(element, values);
            }
        } else {
            values->append(value.toNumber());
        }
    }

    ScriptValueList getFlattenedValues(const QJSValue &value)
    {
        auto values = ScriptValueList();
        getFlattenedValuesRec(value, &values);
        return values;
    }
} // namespace

ScriptEngineJavaScript::ScriptEngineJavaScript(QObject *parent)
    : ScriptEngine(parent)
    , mOnThread(*QThread::currentThread())
    , mJsEngine(new QJSEngine(this))
    , mConsole(new ScriptConsole(this))
{
    mInterruptThread = new QThread();
    mInterruptTimer = new QTimer();
    mInterruptTimer->setInterval(250);
    mInterruptTimer->setSingleShot(true);
    connect(mInterruptTimer, &QTimer::timeout,
        [jsEngine = mJsEngine]() { jsEngine->setInterrupted(true); });
    mInterruptTimer->moveToThread(mInterruptThread);
    mInterruptThread->start();

    mJsEngine->globalObject().setProperty("console",
        mJsEngine->newQObject(mConsole));

    auto file = QFile(":/scripting/ScriptEngineJavaScript.js");
    file.open(QFile::ReadOnly | QFile::Text);
    mJsEngine->evaluate(QTextStream(&file).readAll());
}

ScriptEngineJavaScript::~ScriptEngineJavaScript()
{
    QMetaObject::invokeMethod(mInterruptTimer, "stop",
        Qt::BlockingQueuedConnection);
    connect(mInterruptThread, &QThread::finished, mInterruptThread,
        &QObject::deleteLater);
    mInterruptThread->requestInterruption();
}

void ScriptEngineJavaScript::setOmitReferenceErrors()
{
    mOmitReferenceErrors = true;
}

void ScriptEngineJavaScript::setTimeout(int msec)
{
    mInterruptTimer->setInterval(msec);
}

void ScriptEngineJavaScript::resetInterruptTimer()
{
    Q_ASSERT(&mOnThread == QThread::currentThread());
    QMetaObject::invokeMethod(mInterruptTimer, "start",
        Qt::BlockingQueuedConnection);
    mJsEngine->setInterrupted(false);
}

void ScriptEngineJavaScript::setGlobal(const QString &name, QJSValue value)
{
    Q_ASSERT(&mOnThread == QThread::currentThread());
    mJsEngine->globalObject().setProperty(name, value);
}

void ScriptEngineJavaScript::setGlobal(const QString &name, QObject *object)
{
    Q_ASSERT(&mOnThread == QThread::currentThread());
    mJsEngine->globalObject().setProperty(name, mJsEngine->newQObject(object));
}

void ScriptEngineJavaScript::setGlobal(const QString &name,
    const ScriptValueList &values)
{
    if (values.size() == 1) {
        setGlobal(name, values.at(0));
    } else {
        auto i = 0;
        auto array = mJsEngine->newArray(values.size());
        for (const auto &value : values)
            array.setProperty(i++, value);
        setGlobal(name, array);
    }
}

QJSValue ScriptEngineJavaScript::getGlobal(const QString &name)
{
    Q_ASSERT(&mOnThread == QThread::currentThread());
    return mJsEngine->globalObject().property(name);
}

QJSValue ScriptEngineJavaScript::call(QJSValue &callable,
    const QJSValueList &args, ItemId itemId, MessagePtrSet &messages)
{
    mConsole->setMessages(&messages, itemId);
    resetInterruptTimer();

    auto result = callable.call(args);
    outputError(result, itemId, messages);
    return result;
}

void ScriptEngineJavaScript::validateScript(const QString &script,
    const QString &fileName, MessagePtrSet &messages)
{
    if (script.trimmed().startsWith('{'))
        evaluateScript("json = " + script, fileName, messages);
    else
        evaluateScript("if (false) {" + script + "}", fileName, messages);
}

void ScriptEngineJavaScript::evaluateScript(const QString &script,
    const QString &fileName, MessagePtrSet &messages)
{
    mConsole->setMessages(&messages, fileName);
    resetInterruptTimer();

    auto result = mJsEngine->evaluate(script, fileName);
    if (result.isError())
        messages += MessageList::insert(fileName,
            result.property("lineNumber").toInt(), MessageType::ScriptError,
            result.toString());
}

ScriptValueList ScriptEngineJavaScript::evaluateValues(
    const QString &valueExpression, ItemId itemId, MessagePtrSet &messages)
{
    mConsole->setMessages(&messages, itemId);
    resetInterruptTimer();

    auto result = mJsEngine->evaluate(valueExpression);
    outputError(result, itemId, messages);
    return getFlattenedValues(result);
}

void ScriptEngineJavaScript::outputError(const QJSValue &result, ItemId itemId,
    MessagePtrSet &messages)
{
    if (!result.isError())
        return;

    if (result.errorType() == QJSValue::ErrorType::ReferenceError
        && mOmitReferenceErrors)
        return;

    const auto message = result.toString();
    const auto fileName = result.property("fileName").toString();
    if (!fileName.isEmpty()) {
        const auto lineNumber = result.property("lineNumber").toInt();
        messages += MessageList::insert(
            toNativeCanonicalFilePath(QUrl(fileName).toLocalFile()), lineNumber,
            MessageType::ScriptError, message);
    } else {
        messages +=
            MessageList::insert(itemId, MessageType::ScriptError, message);
    }
}