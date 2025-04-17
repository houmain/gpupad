#include "ScriptEngine.h"
#include "FileDialog.h"
#include "Singletons.h"
#include "objects/ConsoleScriptObject.h"
#include "objects/AppScriptObject.h"
#include "session/SessionModel.h"
#include <QTextStream>
#include <QThread>
#include <QTimer>

#if defined(QtQuick_FOUND)
#  include <QQmlEngine>
#endif

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

ScriptEnginePtr ScriptEngine::make(const QString &basePath, QThread *thread,
    QObject *parent)
{
    auto engine = ScriptEnginePtr(new ScriptEngine(parent));
    if (thread && thread != QThread::currentThread()) {
        engine->moveToThread(thread);
        QMetaObject::invokeMethod(engine.get(), &ScriptEngine::initialize,
            Qt::BlockingQueuedConnection, engine, basePath);
    } else {
        engine->initialize(engine, basePath);
    }
    return engine;
}

ScriptEngine::ScriptEngine(QObject *parent)
    : QObject(parent)
    , mConsoleScriptObject(new ConsoleScriptObject(this))
{
}

void ScriptEngine::initialize(const ScriptEnginePtr &self,
    const QString &basePath)
{
#if defined(QtQuick_FOUND)
    // make a QmlEngine which can be shared with QmlViews
    mJsEngine = new QQmlEngine(this);
#else
    mJsEngine = new QJSEngine(this);
#endif

    mInterruptThread = new QThread();
    mInterruptTimer = new QTimer();
    mInterruptTimer->setSingleShot(true);
    connect(mInterruptTimer, &QTimer::timeout,
        [jsEngine = mJsEngine]() { jsEngine->setInterrupted(true); });
    mInterruptTimer->moveToThread(mInterruptThread);
    mInterruptThread->start();
    setTimeout(5000);

    mJsEngine->installExtensions(QJSEngine::ConsoleExtension);
    setGlobal("console", mConsoleScriptObject);

    auto file = QFile(":/scripting/ScriptEngine.js");
    file.open(QFile::ReadOnly | QFile::Text);
    mJsEngine->evaluate(QTextStream(&file).readAll());

    mAppScriptObject = new AppScriptObject(self, basePath);
    setGlobal("app", mAppScriptObject);
}

ScriptEngine::~ScriptEngine()
{
    Q_ASSERT(QThread::currentThread() == thread());

    QMetaObject::invokeMethod(mInterruptTimer, "stop",
        Qt::BlockingQueuedConnection);
    connect(mInterruptThread, &QThread::finished, mInterruptThread,
        &QObject::deleteLater);
    mInterruptThread->requestInterruption();
}

void ScriptEngine::setOmitReferenceErrors()
{
    mOmitReferenceErrors = true;
}

void ScriptEngine::setTimeout(int msec)
{
    mInterruptTimer->setInterval(msec);
}

void ScriptEngine::resetInterruptTimer()
{
    // TODO: rethink since this interferes with QmlView
#if 0
    Q_ASSERT(&mOnThread == QThread::currentThread());
    QMetaObject::invokeMethod(mInterruptTimer, "start",
        Qt::BlockingQueuedConnection);
    mJsEngine->setInterrupted(false);
#endif
}

void ScriptEngine::setGlobal(const QString &name, QJSValue value)
{
    Q_ASSERT(QThread::currentThread() == thread());
    mJsEngine->globalObject().setProperty(name, value);
}

void ScriptEngine::setGlobal(const QString &name, QObject *object)
{
    Q_ASSERT(QThread::currentThread() == thread());
    mJsEngine->globalObject().setProperty(name, mJsEngine->newQObject(object));
}

void ScriptEngine::setGlobal(const QString &name, const ScriptValueList &values)
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

QJSValue ScriptEngine::getGlobal(const QString &name)
{
    Q_ASSERT(QThread::currentThread() == thread());
    return mJsEngine->globalObject().property(name);
}

QJSValue ScriptEngine::call(QJSValue &callable, const QJSValueList &args,
    ItemId itemId, MessagePtrSet &messages)
{
    Q_ASSERT(QThread::currentThread() == thread());
    mConsoleScriptObject->setMessages(&messages, itemId);
    resetInterruptTimer();

    auto result = callable.call(args);
    outputError(result, itemId, messages);
    return result;
}

void ScriptEngine::validateScript(const QString &script,
    const QString &fileName, MessagePtrSet &messages)
{
    if (script.trimmed().startsWith('{'))
        evaluateScript("json = " + script, fileName, messages);
    else
        evaluateScript("if (false) {" + script + "}", fileName, messages);
}

void ScriptEngine::evaluateScript(const QString &script,
    const QString &fileName, MessagePtrSet &messages)
{
    Q_ASSERT(QThread::currentThread() == thread());
    Q_ASSERT(isNativeCanonicalFilePath(fileName));
    mConsoleScriptObject->setMessages(&messages, fileName);
    resetInterruptTimer();

    auto result = mJsEngine->evaluate(script, fileName);
    outputError(result, 0, messages);
}

ScriptValueList ScriptEngine::evaluateValues(const QString &valueExpression,
    ItemId itemId, MessagePtrSet &messages)
{
    Q_ASSERT(QThread::currentThread() == thread());
    mConsoleScriptObject->setMessages(&messages, itemId);
    resetInterruptTimer();

    auto result = mJsEngine->evaluate(valueExpression);
    outputError(result, itemId, messages);
    return getFlattenedValues(result);
}

void ScriptEngine::outputError(const QJSValue &result, ItemId itemId,
    MessagePtrSet &messages)
{
    if (!result.isError())
        return;

    if (result.errorType() == QJSValue::ErrorType::ReferenceError
        && mOmitReferenceErrors)
        return;

    // clean up message
    auto message = result.property("message").toString();
    message.replace("ScriptObject(", "(");
    static const auto sRemoveAddressRegex =
        QRegularExpression("of object ([^(]+)\\([^)]+\\)");
    message.replace(sRemoveAddressRegex, "of object \\1");

    // a stack trace would also be available
    //const auto stack = result.property("stack").toString();

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

ScriptValueList ScriptEngine::evaluateValues(
    const QStringList &valueExpressions, ItemId itemId, MessagePtrSet &messages)
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
    // fast path, when expression is empty or a number
    if (valueExpression.isEmpty())
        return 0.0;
    auto ok = false;
    auto value = valueExpression.toDouble(&ok);
    if (ok)
        return value;

    const auto values = evaluateValues(valueExpression, itemId, messages);
    return (values.isEmpty() ? 0.0 : values.first());
}

int ScriptEngine::evaluateInt(const QString &valueExpression, ItemId itemId,
    MessagePtrSet &messages)
{
    const auto value = evaluateValue(valueExpression, itemId, messages);
    if (!std::isfinite(value))
        return 0;
    return static_cast<int>(value + 0.5);
}

uint32_t ScriptEngine::evaluateUInt(const QString &valueExpression,
    ItemId itemId, MessagePtrSet &messages)
{
    return static_cast<uint32_t>(
        std::max(evaluateInt(valueExpression, itemId, messages), 0));
}

QJSEngine &ScriptEngine::jsEngine()
{
    Q_ASSERT(QThread::currentThread() == thread());
    return *mJsEngine;
}

void checkValueCount(int valueCount, int offset, int count, ItemId itemId,
    MessagePtrSet &messages)
{
    const auto allowSubset = (offset >= 0);
    if (!allowSubset) {
        // check that value counts match
        if (valueCount != count) {
            // allow setting 4 components of vec3
            if (valueCount != 4 || count != 3)
                messages += MessageList::insert(itemId,
                    MessageType::UniformComponentMismatch,
                    QString("(%1/%2)").arg(valueCount).arg(count));
        }
    } else {
        // check that there are enough values
        if (valueCount < count + offset)
            messages += MessageList::insert(itemId,
                MessageType::UniformComponentMismatch,
                QString("(%1 < %2)").arg(valueCount).arg(count + offset));
    }
}
