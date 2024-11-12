#pragma once

#include "render/RenderSessionBase.h"
#include <QOpenGLVertexArrayObject>

class GLShareSync;
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
    std::shared_ptr<GLShareSync> mShareSync;
    std::unique_ptr<CommandQueue> mCommandQueue;
    std::unique_ptr<CommandQueue> mPrevCommandQueue;
    MessagePtrSet mTimerMessages;
};
