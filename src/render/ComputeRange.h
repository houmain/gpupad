#pragma once

#include "RenderTask.h"
#include "opengl/GLComputeRange.h"

class ComputeRange final : public RenderTask
{
    Q_OBJECT
public:
    explicit ComputeRange(RendererPtr renderer, QObject *parent = nullptr)
        : RenderTask(std::move(renderer), parent)
    {
        Q_ASSERT(RenderTask::renderer().api() == RenderAPI::OpenGL);
    }

    ~ComputeRange() override { releaseResources(); }

    void setImage(QOpenGLTexture::Target target, GLuint textureId,
        const TextureData &texture, int level, int layer, int face)
    {
        mImpl.setImage(target, textureId, texture, level, layer, face);
    }

Q_SIGNALS:
    void rangeComputed(const Range &range);

private:
    void render() override { mImpl.render(); }

    void finish() override { Q_EMIT rangeComputed(mImpl.range()); }

    void release() override { mImpl.release(); }

    GLComputeRange mImpl;
};
