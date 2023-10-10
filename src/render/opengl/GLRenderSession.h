#pragma once

#include "../RenderSessionBase.h"
#include <QOpenGLVertexArrayObject>

class QOpenGLTimerQuery;

class GLRenderSession final : public RenderSessionBase
{
public:
    GLRenderSession();
    ~GLRenderSession();

    void render() override;
    void finish() override;
    void release() override;

private:
    struct CommandQueue;
    struct GroupIteration;

    void createCommandQueue();
    void reuseUnmodifiedItems();
    void executeCommandQueue();
    void setNextCommandQueueIndex(int index);
    void downloadModifiedResources();
    void outputTimerQueries();

    QOpenGLVertexArrayObject mVao;
    QScopedPointer<CommandQueue> mCommandQueue;
    QScopedPointer<CommandQueue> mPrevCommandQueue;
    int mNextCommandQueueIndex{ };
    QMap<ItemId, GroupIteration> mGroupIterations;
    QList<std::pair<ItemId, std::shared_ptr<const QOpenGLTimerQuery>>> mTimerQueries;
    MessagePtrSet mTimerMessages;
};
