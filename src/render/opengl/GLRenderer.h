
#include "render/Renderer.h"
#include <QObject>
#include <QThread>

class GLRenderer : public QObject, public Renderer
{
    Q_OBJECT
public:
    explicit GLRenderer(QObject *parent = nullptr);
    ~GLRenderer() override;

    void render(RenderTask *task) override;
    void release(RenderTask *task) override;
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
};

QString getFirstGLError();
