#pragma once

#include "Evaluation.h"
#include "Renderer.h"
#include <QThread>
#include <QSet>
#include <optional>

using ItemId = int;

class RenderTask : public QObject
{
    Q_OBJECT
public:
    explicit RenderTask(RendererPtr renderer, QObject *parent = nullptr);
    RenderTask(const RenderTask &) = delete;
    RenderTask &operator=(const RenderTask &) = delete;
    ~RenderTask() override;

    virtual QSet<ItemId> usedItems() const { return {}; }
    bool updating() const { return mUpdating; }

    Renderer &renderer() { return *mRenderer; }
    void update(bool itemChanged = false,
        EvaluationType evaluationType = EvaluationType::Reset);

Q_SIGNALS:
    void updated();

protected:
    void releaseResources();

    template <typename F>
    void dispatchToRenderThread(F &&function)
    {
        if (QThread::currentThread() != thread()) {
            QMetaObject::invokeMethod(this, std::forward<F>(function),
                Qt::BlockingQueuedConnection);
        } else {
            function();
        }
    }

private:
    friend class GLRenderer;
    friend class VKRenderer;

    // 1. called per update in main thread
    virtual void prepare(bool itemsChanged, EvaluationType evaluationType) { }

    // 2. called per update in render thread
    virtual void configure() { }

    // 3. called per update in main thread
    virtual void configured() { }

    // 4. called per update in render thread
    virtual void render() = 0;

    // 5. called per update in main thread
    void handleRendered();
    virtual void finish() { }

    // 6. called once in render thread
    virtual void release() { }

    RendererPtr mRenderer;
    bool mUpdating{};
    bool mItemsChanged{};
    std::optional<EvaluationType> mPendingEvaluation;
};
