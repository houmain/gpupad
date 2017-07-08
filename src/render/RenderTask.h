#ifndef RENDERTASK_H
#define RENDERTASK_H

#include <QObject>
#include <QSet>

using ItemId = int;
class GLContext;

class RenderTask : public QObject
{
    Q_OBJECT
public:
    explicit RenderTask(QObject *parent = nullptr);
    virtual ~RenderTask();

    virtual QSet<ItemId> usedItems() const = 0;

    void update(bool itemChanged, bool manualEvaluation);

signals:
    void updated();

protected:
    void releaseResources();

private:
    friend class Renderer;

    // 1. called in main thread
    virtual void prepare(bool itemsChanged, bool manualEvaluation) = 0;

    // 2. called in render thread
    virtual void render() = 0;

    // 3. called in main thread
    void handleRendered();
    virtual void finish() = 0;

    // 4. called in render thread
    virtual void release() = 0;

    bool mReleased{ };
    bool mUpdating{ };
    bool mItemsChanged{ };
    bool mManualEvaluation{ };
};

#endif // RENDERTASK_H
