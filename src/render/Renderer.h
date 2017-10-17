#ifndef RENDERER_H
#define RENDERER_H

#include <QObject>
#include <QThread>

class RenderTask;

class Renderer : public QObject
{
    Q_OBJECT
public:
    explicit Renderer(QObject *parent = nullptr);
    ~Renderer();

    void render(RenderTask *task);
    void release(RenderTask *task);

signals:
    void renderTask(RenderTask* renderTask, QPrivateSignal);
    void releaseTask(RenderTask *renderTask, void *userData, QPrivateSignal);

private slots:
    void handleTaskRendered();

private:
    class Worker;

    void renderNextTask();

    QThread mThread;
    QScopedPointer<Worker> mWorker;
    QList<RenderTask*> mPendingTasks;
    RenderTask* mCurrentTask{ };
};

#endif // RENDERER_H
