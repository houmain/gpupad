#ifndef RENDERER_H
#define RENDERER_H

#include <QObject>
#include <QThread>

class RenderTask;

class Renderer final : public QObject
{
    Q_OBJECT
public:
    explicit Renderer(QObject *parent = nullptr);
    ~Renderer() override;

    void render(RenderTask *task);
    void release(RenderTask *task);

Q_SIGNALS:
    void renderTask(RenderTask* renderTask, QPrivateSignal);
    void releaseTask(RenderTask *renderTask, void *userData, QPrivateSignal);

private:
    class Worker;
    void handleTaskRendered();
    void renderNextTask();

    QThread mThread;
    QScopedPointer<Worker> mWorker;
    QList<RenderTask*> mPendingTasks;
    RenderTask* mCurrentTask{ };
};

QString getFirstGLError();

#endif // RENDERER_H
