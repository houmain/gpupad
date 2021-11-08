#include "ScriptEngine.h"
#include "Singletons.h"
#include "session/SessionModel.h"
#include "FileDialog.h"
#include <QThread>
#include <QTimer>

JSConsole::JSConsole(QObject *parent) :
    QObject(parent)
{
}

void JSConsole::setMessages(MessagePtrSet *messages, const QString &fileName)
{
    mMessages = messages;
    mFileName = fileName;
    mItemId = { };
}

void JSConsole::setMessages(MessagePtrSet *messages, ItemId itemId)
{
    mMessages = messages;
    mItemId = itemId;
    mFileName.clear();
}

void JSConsole::log(QString message)
{
    (*mMessages) += (!mFileName.isEmpty() ?
        MessageList::insert(mFileName, 0,
            MessageType::ScriptMessage, message) :
        MessageList::insert(
            mItemId, MessageType::ScriptMessage, message));
}

ScriptValue evaluateValueExpression(const QString &expression, bool *ok)
{
    auto tmp = false;
    if (!ok)
        ok = &tmp;

    if (expression.isEmpty()) {
        *ok = true;
        return 0;
    }
        
    const auto value = expression.toDouble(ok);
    if (*ok)
        return value;

    static QJSEngine sJsEngine;
    const auto result = sJsEngine.evaluate(expression);
    if (result.isError()) {
       *ok = false;
       return 0;
    }
    return result.toVariant().toDouble(ok);
}

int evaluateIntExpression(const QString &expression, bool *ok)
{
    return static_cast<int>(evaluateValueExpression(expression, ok) + 0.5);
}

int ScriptVariable::count() const
{
    return (mValues ? mValues->size() : 0);
}

ScriptValue ScriptVariable::get() const
{
    return (mValues && !mValues->isEmpty() ? mValues->first() : 0.0);
}

ScriptValue ScriptVariable::get(int index) const
{
    return (mValues && index < mValues->size() ? mValues->at(index) : 0.0);
}

ScriptEngine::ScriptEngine(QObject *parent)
    : QObject(parent)
    , mOnThread(*QThread::currentThread())
    , mJsEngine(new QJSEngine(this))
    , mConsole(new JSConsole(this))
{
#if (QT_VERSION >= QT_VERSION_CHECK(5, 14, 0))
    mInterruptThread = new QThread();
    mInterruptTimer = new QTimer();
    mInterruptTimer->setInterval(1000);
    mInterruptTimer->setSingleShot(true);
    connect(mInterruptTimer, &QTimer::timeout,
        [jsEngine = mJsEngine]() { jsEngine->setInterrupted(true); });
    mInterruptTimer->moveToThread(mInterruptThread);
    mInterruptThread->start();
#endif

    mJsEngine->globalObject().setProperty("console",
        mJsEngine->newQObject(mConsole));

    evaluate(
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

ScriptEngine::~ScriptEngine() 
{
#if (QT_VERSION >= QT_VERSION_CHECK(5, 14, 0))
    QMetaObject::invokeMethod(mInterruptTimer, "stop", Qt::BlockingQueuedConnection);
    connect(mInterruptThread, &QThread::finished, mInterruptThread, &QObject::deleteLater);
    mInterruptThread->requestInterruption();
#endif
}

QJSValue ScriptEngine::evaluate(const QString &program, const QString &fileName, int lineNumber)
{
    Q_ASSERT(&mOnThread == QThread::currentThread());
#if (QT_VERSION >= QT_VERSION_CHECK(5, 14, 0))
    QMetaObject::invokeMethod(mInterruptTimer, "start", Qt::BlockingQueuedConnection);
    mJsEngine->setInterrupted(false);
#endif
    return mJsEngine->evaluate(program, fileName, lineNumber);
}

void ScriptEngine::setGlobal(const QString &name, QJSValue value)
{
    Q_ASSERT(&mOnThread == QThread::currentThread());
    mJsEngine->globalObject().setProperty(name, value);
}

void ScriptEngine::setGlobal(const QString &name, QObject *object)
{
    Q_ASSERT(&mOnThread == QThread::currentThread());
    mJsEngine->globalObject().setProperty(name, mJsEngine->newQObject(object));
}

void ScriptEngine::setGlobal(const QString &name, const ScriptValueList &values)
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

QJSValue ScriptEngine::getGlobal(const QString &name)
{
    Q_ASSERT(&mOnThread == QThread::currentThread());
    return mJsEngine->globalObject().property(name);
}

QJSValue ScriptEngine::call(QJSValue &callable, const QJSValueList &args,
    ItemId itemId, MessagePtrSet &messages)
{
    mConsole->setMessages(&messages, itemId);

    auto result = callable.call(args);
    if (result.isError())
        messages += MessageList::insert(
            itemId, MessageType::ScriptError, result.toString());
    return result;
}

void ScriptEngine::evaluateScript(const QString &script,
    const QString &fileName, MessagePtrSet &messages)
{
    mConsole->setMessages(&messages, fileName);

    auto result = evaluate(script, fileName);
    if (result.isError())
        messages += MessageList::insert(
            fileName, result.property("lineNumber").toInt(),
            MessageType::ScriptError, result.toString());
}

ScriptValueList ScriptEngine::evaluateValues(const QStringList &valueExpressions,
    ItemId itemId, MessagePtrSet &messages)
{
    mConsole->setMessages(&messages, itemId);

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

        auto result = evaluate(valueExpression);
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
    return values;
}

ScriptValue ScriptEngine::evaluateValue(const QString &valueExpression,
    ItemId itemId, MessagePtrSet &messages) 
{
    const auto values = evaluateValues({ valueExpression },
        itemId, messages);
    return (values.isEmpty() ? 0.0 : values.first());
}

int ScriptEngine::evaluateInt(const QString &valueExpression,
      ItemId itemId, MessagePtrSet &messages)
{
    return static_cast<int>(evaluateValue(valueExpression, itemId, messages) + 0.5);
}

void ScriptEngine::updateVariables(MessagePtrSet &messages)
{
    for (auto it = mVariables.begin(); it != mVariables.end(); )
        if (updateVariable(*it, 0, messages)) {
            ++it;
        }
        else {
            it = mVariables.erase(it);
        }
}

bool ScriptEngine::updateVariable(const Variable &variable,
    ItemId itemId, MessagePtrSet &messages)
{
    auto values = variable.values.lock();
    if (!values)
        return false;
    *values = evaluateValues(variable.expressions, itemId, messages);

    // set global in script state, when variable name is known
    if (!variable.name.isEmpty())
        setGlobal(variable.name, *values);
    return true;
}

ScriptVariable ScriptEngine::getVariable(const QString &variableName,
    const QStringList &valueExpressions,
    ItemId itemId, MessagePtrSet &messages)
{
    auto values = QSharedPointer<ScriptValueList>(new ScriptValueList());
    mVariables.append({ variableName, valueExpressions, values });
    updateVariable(mVariables.back(), itemId, messages);

    auto result = ScriptVariable();
    result.mValues = std::move(values);
    return result;
}

ScriptVariable ScriptEngine::getVariable(const QString &valueExpression,
    ItemId itemId, MessagePtrSet &messages)
{
    return getVariable("", QStringList{ valueExpression }, itemId, messages);
}
