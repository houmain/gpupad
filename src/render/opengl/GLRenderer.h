#pragma once

#include "render/Renderer.h"
#include <QThread>

class GLRenderer : public QObject, public Renderer
{
    Q_OBJECT

#if defined(OPENGL_ENABLED)
public:
    explicit GLRenderer(QObject *parent = nullptr);
    ~GLRenderer() override;

    void render(RenderTask *task) override;
    void release(RenderTask *task) override;
    void finish() override;
    QThread* renderThread() override { return &mThread; }

Q_SIGNALS:
    void configureTask(RenderTask *renderTask, QPrivateSignal);
    void renderTask(RenderTask *renderTask, QPrivateSignal);
    void releaseTask(RenderTask *renderTask, void *userData, QPrivateSignal);

private:
    class Worker;
    void handleTaskConfigured();
    void handleTaskRendered();
    void renderNextTask();

    QThread mThread;
    std::unique_ptr<Worker> mWorker;
    QList<RenderTask *> mPendingTasks;
    RenderTask *mCurrentTask{};

#else // !defined(OPENGL_ENABLED)

public:
    GLRenderer(QObject *parent = nullptr)
        : QObject(parent)
        , Renderer(Renderer::Type::OpenGL)
    {
        mMessages.insert(0, MessageType::OpenGLVersionNotAvailable);
        setFailed();
    }

    ~GLRenderer() = default;
    void render(RenderTask *task) override { }
    void release(RenderTask *task) override { }
    void finish() override { }
    QThread *renderThread() override { return nullptr; }

private:
    MessagePtrSet mMessages;

#endif // !defined(OPENGL_ENABLED)
};

QString getFirstGLError();
