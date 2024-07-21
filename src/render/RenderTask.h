#pragma once

#include "Evaluation.h"
#include "Renderer.h"
#include <QObject>
#include <QSet>
#include <optional>

using ItemId = int;

class RenderTask : public QObject
{
    Q_OBJECT
public:
    explicit RenderTask(QObject *parent = nullptr);
    ~RenderTask() override;

    virtual QSet<ItemId> usedItems() const { return {}; }

    void update(RendererPtr renderer, bool itemChanged = false,
        EvaluationType evaluationType = EvaluationType::Reset);

Q_SIGNALS:
    void updated();

protected:
    Renderer &renderer() { return *mRenderer; }
    void releaseResources();

private:
    friend class GLRenderer;
    friend class VKRenderer;

    // 0. called once in main thread
    virtual bool initialize() { return true; }

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
