#pragma once

#include "render/RenderSessionBase.h"
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

    void createCommandQueue();
    void reuseUnmodifiedItems();
    void executeCommandQueue();
    void downloadModifiedResources();
    void outputTimerQueries();

    QOpenGLVertexArrayObject mVao;
    QScopedPointer<CommandQueue> mCommandQueue;
    QScopedPointer<CommandQueue> mPrevCommandQueue;
    MessagePtrSet mTimerMessages;
};
