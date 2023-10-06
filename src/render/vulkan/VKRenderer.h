
#include <QObject>
#include <QThread>
#include "../Renderer.h"

namespace KDGpu { class Device; }

class VKRenderer : public QObject, public Renderer
{
    Q_OBJECT
public:
    explicit VKRenderer(QObject *parent = nullptr);
    ~VKRenderer() override;

    void render(RenderTask *task) override;
    void release(RenderTask *task) override;

    KDGpu::Device *device() { return mDevice; }

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
    RenderTask *mCurrentTask{ };
    KDGpu::Device *mDevice{ };
};
