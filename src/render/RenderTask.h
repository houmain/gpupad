#ifndef RENDERTASK_H
#define RENDERTASK_H

#include <QObject>
#include <QSet>

using ItemId = int;
class QOpenGLContext;

class RenderTask : public QObject
{
    Q_OBJECT
public:
    explicit RenderTask(QObject *parent = nullptr);
    virtual ~RenderTask();

    virtual QSet<ItemId> usedItems() const = 0;

    void update();

signals:
    void updated();

protected:
    void releaseResources();

private:
    friend class Renderer;
    friend class BackgroundRenderer;

    // 1. called in main thread
    virtual void prepare() = 0;

    // 2. called in render thread
    virtual void render(QOpenGLContext &glContext) = 0;

    // 3. called in main thread
    virtual void finish() = 0;
    void validate();

    // 4. called in render thread
    virtual void release(QOpenGLContext &glContext) = 0;

    bool mReleased{ };
    int mInvalidationCount{ };
};

#endif // RENDERTASK_H
