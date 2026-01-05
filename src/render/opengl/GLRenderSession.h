#pragma once

#include "render/RenderSessionBase.h"
#include <QOpenGLVertexArrayObject>
#include <QOpenGLTimerQuery>
#include <deque>

class GLShareSync;

class GLRenderSession final : public RenderSessionBase
{
public:
    struct CommandQueue;

    GLRenderSession(RendererPtr renderer, const QString &basePath);
    ~GLRenderSession();

    void render() override;
    void finish() override;
    void release() override;
    quint64 getTextureHandle(ItemId itemId) override;
    std::vector<Duration> resetTimeQueries(size_t count) override;
    std::shared_ptr<void> beginTimeQuery(size_t index) override;

private:
    void createCommandQueue();

    QOpenGLVertexArrayObject mVao;
    std::shared_ptr<GLShareSync> mShareSync;
    std::unique_ptr<CommandQueue> mCommandQueue;
    std::unique_ptr<CommandQueue> mPrevCommandQueue;
    std::deque<QOpenGLTimerQuery> mTimeQueries;
};
