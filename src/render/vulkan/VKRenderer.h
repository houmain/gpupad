
#include "render/Renderer.h"
#include <QObject>
#include <QThread>

namespace KDGpu {
    class Device;
}
struct ktxVulkanDeviceInfo;

class VKRenderer : public QObject, public Renderer
{
    Q_OBJECT
public:
    explicit VKRenderer(QObject *parent = nullptr);
    ~VKRenderer() override;

    void render(RenderTask *task) override;
    void release(RenderTask *task) override;

    KDGpu::Device &device();
    ktxVulkanDeviceInfo &ktxDeviceInfo();

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
    KDGpu::Device *mDevice{};
    ktxVulkanDeviceInfo *mKtxDeviceInfo{};
};
