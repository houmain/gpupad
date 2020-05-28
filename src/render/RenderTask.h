#ifndef RENDERTASK_H
#define RENDERTASK_H

#include <QObject>
#include <QSet>
#include "Evaluation.h"

using ItemId = int;

class RenderTask : public QObject
{
    Q_OBJECT
public:
    explicit RenderTask(QObject *parent = nullptr);
    ~RenderTask() override;

    virtual QSet<ItemId> usedItems() const = 0;

    void update(bool itemChanged = false,
        EvaluationType evaluationType = EvaluationType::Reset);

signals:
    void updated();

protected:
    void releaseResources();

private:
    friend class Renderer;

    // 1. called in main thread
    virtual void prepare(bool itemsChanged,
        EvaluationType evaluationType) = 0;

    // 2. called in render thread
    virtual void render() = 0;

    // 3. called in main thread
    void handleRendered();
    virtual void finish() = 0;

    // 4. called in render thread
    virtual void release() = 0;

    bool mPrepared{ };
    bool mReleased{ };
    bool mUpdating{ };
    bool mItemsChanged{ };
    std::optional<EvaluationType> mPendingEvaluation;
};

#endif // RENDERTASK_H
