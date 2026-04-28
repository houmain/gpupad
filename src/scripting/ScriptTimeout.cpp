
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
        QMetaObject::invokeMethod(
            &mTimer, [timer = &mTimer]() { timer->start(); },
            Qt::BlockingQueuedConnection);
    }

    void cancelTimeout()
    {
        QMetaObject::invokeMethod(
            &mTimer, [timer = &mTimer]() { timer->stop(); },
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

std::shared_ptr<void> suspendScriptEngineTimeout()
{
    const auto lock = QMutexLocker(&gRunningScriptEnginesMutex);
    if (!gScriptTimeout)
        return {};

    if (gScriptEnginesRunningOnMainThread)
        gScriptTimeout->cancelTimeout();
    return std::shared_ptr<void>(nullptr, [](void *) {
        if (gScriptEnginesRunningOnMainThread)
            gScriptTimeout->startTimeout();
    });
}

void registerRunningScriptEngine(ScriptEngine *scriptEngine)
{
    {
        const auto lock = QMutexLocker(&gRunningScriptEnginesMutex);
        gRunningScriptEngines.append(scriptEngine);
    }
    if (onMainThread() && gScriptEnginesRunningOnMainThread++ == 0)
        if (gScriptTimeout)
            gScriptTimeout->startTimeout();
}

void deregisterRunningScriptEngine(ScriptEngine *scriptEngine)
{
    {
        const auto lock = QMutexLocker(&gRunningScriptEnginesMutex);
        gRunningScriptEngines.removeOne(scriptEngine);
    }
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
