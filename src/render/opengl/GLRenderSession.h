#pragma once
#if defined(OPENGL_ENABLED)

#  include "render/RenderSessionBase.h"
#  include <QOpenGLTimerQuery>
#  include <deque>

class GLRenderSession final : public RenderSessionBase
{
public:
    struct CommandQueue;

    explicit GLRenderSession(RendererPtr renderer);
    ~GLRenderSession();

    void render() override;
    void finish() override;
    void release() override;
    quint64 getTextureHandle(ItemId itemId) override;
    std::vector<Duration> resetTimeQueries(size_t count) override;
    std::shared_ptr<void> beginTimeQuery(size_t index) override;

private:
    void createCommandQueue();

    std::unique_ptr<CommandQueue> mCommandQueue;
    std::unique_ptr<CommandQueue> mPrevCommandQueue;
    std::deque<QOpenGLTimerQuery> mTimeQueries;
};

#endif // !defined(OPENGL_ENABLED)
