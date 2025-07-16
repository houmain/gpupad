#pragma once

#include "render/RenderSessionBase.h"
#include <QOpenGLVertexArrayObject>

class GLShareSync;
class QOpenGLTimerQuery;

class GLRenderSession final : public RenderSessionBase
{
public:
    GLRenderSession(RendererPtr renderer, const QString &basePath);
    ~GLRenderSession();

    void render() override;
    void finish() override;
    void release() override;
    quint64 getTextureHandle(ItemId itemId) override;

private:
    struct CommandQueue;

    void createCommandQueue();
    void buildCommandQueue();
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
