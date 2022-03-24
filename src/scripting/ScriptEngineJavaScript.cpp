#include "ScriptEngineJavaScript.h"
#include "Singletons.h"
#include "session/SessionModel.h"
#include "FileDialog.h"
#include "ScriptConsole.h"
#include <QTextStream>
#include <QThread>
#include <QTimer>

ScriptEngineJavaScript::ScriptEngineJavaScript(QObject *parent)
    : ScriptEngine(parent)
    , mOnThread(*QThread::currentThread())
    , mConsole(new ScriptConsole(this))
    , mJsEngine(new QJSEngine(this))
{
#if (QT_VERSION >= QT_VERSION_CHECK(5, 14, 0))
    mInterruptThread = new QThread();
    mInterruptTimer = new QTimer();
    mInterruptTimer->setInterval(250);
    mInterruptTimer->setSingleShot(true);
    connect(mInterruptTimer, &QTimer::timeout,
        [jsEngine = mJsEngine]() { jsEngine->setInterrupted(true); });
    mInterruptTimer->moveToThread(mInterruptThread);
    mInterruptThread->start();
#endif

    mJsEngine->globalObject().setProperty("console",
        mJsEngine->newQObject(mConsole));

    auto file = QFile(":/scripting/ScriptEngineJavaScript.js");
    file.open(QFile::ReadOnly | QFile::Text);
    mJsEngine->evaluate(QTextStream(&file).readAll());
}

ScriptEngineJavaScript::~ScriptEngineJavaScript() 
{
#if (QT_VERSION >= QT_VERSION_CHECK(5, 14, 0))
    QMetaObject::invokeMethod(mInterruptTimer, "stop", Qt::BlockingQueuedConnection);
    connect(mInterruptThread, &QThread::finished, mInterruptThread, &QObject::deleteLater);
    mInterruptThread->requestInterruption();
#endif
}

void ScriptEngineJavaScript::setTimeout(int msec)
{
#if (QT_VERSION >= QT_VERSION_CHECK(5, 14, 0))
    mInterruptTimer->setInterval(msec);
#endif
}

QJSValue ScriptEngineJavaScript::evaluate(const QString &program, const QString &fileName, int lineNumber)
{
    Q_ASSERT(&mOnThread == QThread::currentThread());
#if (QT_VERSION >= QT_VERSION_CHECK(5, 14, 0))
    QMetaObject::invokeMethod(mInterruptTimer, "start", Qt::BlockingQueuedConnection);
    mJsEngine->setInterrupted(false);
#endif
    return mJsEngine->evaluate(program, fileName, lineNumber);
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

void ScriptEngineJavaScript::setGlobal(const QString &name, const ScriptValueList &values)
{
    if (values.size() == 1) {
        setGlobal(name, values.at(0));
    }
    else {
        auto i = 0;
        auto array = QJSValue();
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

QJSValue ScriptEngineJavaScript::call(QJSValue &callable, const QJSValueList &args,
    ItemId itemId, MessagePtrSet &messages)
{
    mConsole->setMessages(&messages, itemId);

    auto result = callable.call(args);
    if (result.isError())
        messages += MessageList::insert(
            itemId, MessageType::ScriptError, result.toString());
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

    auto result = evaluate(script, fileName);
    if (result.isError())
        messages += MessageList::insert(
            fileName, result.property("lineNumber").toInt(),
            MessageType::ScriptError, result.toString());
}

ScriptValueList ScriptEngineJavaScript::evaluateValues(
    const QString &valueExpression,
    ItemId itemId, MessagePtrSet &messages)
{
    mConsole->setMessages(&messages, itemId);

    auto result = evaluate(valueExpression);
    if (result.isError())
        messages += MessageList::insert(
            itemId, MessageType::ScriptError, result.toString());

    auto values = ScriptValueList();
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
    return values;
}

