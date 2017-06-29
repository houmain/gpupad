#ifndef RENDERSESSION_H
#define RENDERSESSION_H

#include "RenderTask.h"
#include "GLContext.h"
#include <QScopedPointer>
#include <QSet>

class RenderSession : public RenderTask
{
public:
    explicit RenderSession(QObject *parent = nullptr);
    ~RenderSession();

    QSet<ItemId> usedItems() const override { return mUsedItems; }

private:
    void prepare() override;
    void render(QOpenGLContext &glContext) override;
    void finish() override;
    void release(QOpenGLContext &glContext) override;

    QScopedPointer<GLContext> mContext;
    QSet<ItemId> mUsedItems;
};

#endif // RENDERSESSION_H
