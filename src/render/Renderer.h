#pragma once

#include "AdapterIdentity.h"
#include "Device.h"
#include "MessageList.h"
#include "session/ItemEnums.h"
#include <QList>
#include <QThread>
#include <memory>

using RendererPtr = std::shared_ptr<class Renderer>;
class RenderTask;

class Renderer : public QObject
{
    Q_OBJECT

public:
    using Type = ItemEnums::Renderer;

    Renderer(Type type, std::unique_ptr<Device> device,
        const AdapterIdentity &adapterIdentity, QObject *parent = nullptr);
    explicit Renderer(Type type, MessageType failureMessage,
        QObject *parent = nullptr);
    ~Renderer() override;

    QThread *renderThread();

    Type type() const { return mType; }
    bool failed() const { return mFailed; }
    void finish();
    Device &device();
    const Device &device() const;

    template <typename T>
    T &device()
    {
        return static_cast<T &>(device());
    }

    template <typename T>
    const T &device() const
    {
        return static_cast<const T &>(device());
    }

Q_SIGNALS:
    void configureTaskRequested(RenderTask *renderTask, QPrivateSignal);
    void renderTaskRequested(RenderTask *renderTask, QPrivateSignal);
    void releaseTaskRequested(RenderTask *renderTask, void *userData,
        QPrivateSignal);

protected:
    void setFailed() { mFailed = true; }

private:
    friend class RenderTask;

    class Worker;

    void render(RenderTask *task);
    void release(RenderTask *task);
    void renderNextTask();
    void handleTaskConfigured();
    void handleTaskRendered();

    void configureRenderTask(RenderTask *task);
    void renderRenderTask(RenderTask *task);
    void releaseRenderTask(RenderTask *task);
    void configuredRenderTask(RenderTask *task);
    void finishRenderTask(RenderTask *task);

    const Type mType;
    bool mFailed{};
    QThread mThread;
    std::unique_ptr<Worker> mWorker;
    QList<RenderTask *> mPendingTasks;
    RenderTask *mCurrentTask{};
    MessagePtrSet mMessages;
};
