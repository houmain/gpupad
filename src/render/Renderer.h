#ifndef RENDERER_H
#define RENDERER_H

#include "RenderTask.h"
#include <QObject>
#include <QThread>

class BackgroundRenderer;

class Renderer : public QObject
{
    Q_OBJECT
public:
    explicit Renderer(QObject *parent = 0);
    ~Renderer();

    void render(RenderTask *task);
    void release(RenderTask *task);

signals:
    void renderTask(RenderTask* renderTask, QPrivateSignal);
    void releaseTask(RenderTask *renderTask, void *userData, QPrivateSignal);

private slots:
    void handleTaskRendered();

private:
    void renderNextTask();

    QThread mThread;
    QScopedPointer<BackgroundRenderer> mBackgroundRenderer;
    QList<RenderTask*> mPendingTasks;
    RenderTask* mCurrentTask{ };
};

#endif // RENDERER_H
