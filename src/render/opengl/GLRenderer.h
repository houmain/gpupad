
#include <QObject>
#include <QThread>
#include "render/Renderer.h"

class GLRenderer : public QObject, public Renderer
{
    Q_OBJECT
public:
    explicit GLRenderer(QObject *parent = nullptr);
    ~GLRenderer() override;

    void render(RenderTask *task) override;
    void release(RenderTask *task) override;

Q_SIGNALS:
    void configureTask(RenderTask* renderTask, QPrivateSignal);
    void renderTask(RenderTask* renderTask, QPrivateSignal);
    void releaseTask(RenderTask *renderTask, void *userData, QPrivateSignal);

private:
    class Worker;
    void handleTaskConfigured();
    void handleTaskRendered();
    void renderNextTask();

    QThread mThread;
    QScopedPointer<Worker> mWorker;
    QList<RenderTask*> mPendingTasks;
    RenderTask* mCurrentTask{ };
};

QString getFirstGLError();
