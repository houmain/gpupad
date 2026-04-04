
#include "ScriptTimeout.h"
#include "Singletons.h"
#include "ScriptEngine.h"
#include <QMutex>
#include <QList>
#include <QThread>
#include <QTimer>

class ScriptTimeout : public QObject
{
    Q_OBJECT
public:
    explicit ScriptTimeout(std::chrono::milliseconds timeout)
    {
        mTimer.setSingleShot(true);
        mTimer.setInterval(timeout);
        connect(&mTimer, &QTimer::timeout, &interruptRunningScriptEngines);
        mTimer.moveToThread(&mThread);
        mThread.start();
    }

    ~ScriptTimeout()
    {
        mThread.quit();
        mThread.wait();
    }

    void startTimeout()
    {
        QMetaObject::invokeMethod(&mTimer, "start",
            Qt::BlockingQueuedConnection);
    }

    void cancelTimeout()
    {
        QMetaObject::invokeMethod(&mTimer, "stop",
            Qt::BlockingQueuedConnection);
    }

private:
    QThread mThread;
    QTimer mTimer;
};

namespace {
    QMutex gRunningScriptEnginesMutex;
    QList<ScriptEngine *> gRunningScriptEngines;
    int gScriptEnginesRunningOnMainThread;
    std::unique_ptr<ScriptTimeout> gScriptTimeout;
} // namespace

void setScriptEngineTimeout(std::chrono::milliseconds timeout)
{
    gScriptTimeout = std::make_unique<ScriptTimeout>(timeout);
}

void registerRunningScriptEngine(ScriptEngine *scriptEngine)
{
    const auto lock = QMutexLocker(&gRunningScriptEnginesMutex);
    gRunningScriptEngines.append(scriptEngine);

    if (onMainThread() && gScriptEnginesRunningOnMainThread++ == 0)
        if (gScriptTimeout)
            gScriptTimeout->startTimeout();
}

void deregisterRunningScriptEngine(ScriptEngine *scriptEngine)
{
    const auto lock = QMutexLocker(&gRunningScriptEnginesMutex);
    gRunningScriptEngines.removeOne(scriptEngine);

    if (onMainThread() && --gScriptEnginesRunningOnMainThread == 0)
        if (gScriptTimeout)
            gScriptTimeout->cancelTimeout();
}

void interruptRunningScriptEngines()
{
    const auto lock = QMutexLocker(&gRunningScriptEnginesMutex);
    for (auto *scriptEngine : gRunningScriptEngines)
        scriptEngine->interrupt();
}

#include "ScriptTimeout.moc"
