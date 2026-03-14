#pragma once

#include "render/Renderer.h"
#include "MessageList.h"
#include <QThread>

struct ID3D12Device;
struct ID3D12CommandQueue;
struct ID3D12CommandAllocator;
class RenderTargetHelper;

class D3DRenderer : public QObject, public Renderer
{
    Q_OBJECT

Q_SIGNALS:
    void configureTask(RenderTask *renderTask, QPrivateSignal);
    void renderTask(RenderTask *renderTask, QPrivateSignal);
    void releaseTask(RenderTask *renderTask, void *userData, QPrivateSignal);

#if defined(_WIN32)
public:
    explicit D3DRenderer(QObject *parent = nullptr);
    ~D3DRenderer() override;

    void render(RenderTask *task) override;
    void release(RenderTask *task) override;
    QThread *renderThread() override { return &mThread; }

    ID3D12Device &device();
    ID3D12CommandQueue &queue();
    RenderTargetHelper &renderTargetHelper();

private:
    class Worker;
    void handleTaskConfigured();
    void handleTaskRendered();
    void renderNextTask();

    QThread mThread;
    std::unique_ptr<Worker> mWorker;
    QList<RenderTask *> mPendingTasks;
    RenderTask *mCurrentTask{};

    ID3D12Device *mDevice{};
    ID3D12CommandQueue *mQueue{};
    RenderTargetHelper *mRenderTargetHelper{};

#else // !_WIN32

public:
    D3DRenderer(QObject *parent = nullptr)
        : QObject(parent)
        , Renderer(RenderAPI::Direct3D)
    {
        mMessages += MessageList::insert(0, MessageType::Direct3DNotAvailable);
        setFailed();
    }

    ~D3DRenderer() = default;
    void render(RenderTask *task) { }
    void release(RenderTask *task) { }
    QThread *renderThread() override { return nullptr; }

private:
    MessagePtrSet mMessages;

#endif // !_WIN32
};
