#pragma once

#include "RenderTask.h"
#include "opengl/GLDevice.h"
#include "opengl/GLComputeRange.h"

#if defined(OPENGL_ENABLED)

class ComputeRange final : public RenderTask
{
    Q_OBJECT
public:
    explicit ComputeRange(RendererPtr renderer, QObject *parent = nullptr)
        : RenderTask(std::move(renderer), parent)
    {
        Q_ASSERT(RenderTask::renderer().type() == Renderer::Type::OpenGL);
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
    void render() override
    {
        auto &gl = renderer().device<GLDevice>().context();
        mImpl.render(gl);
    }

    void finish() override { Q_EMIT rangeComputed(mImpl.range()); }

    void release() override { mImpl.release(); }

    GLComputeRange mImpl;
};

#else // !defined(OPENGL_ENABLED)

class ComputeRange final : public RenderTask
{
    Q_OBJECT
public:
    explicit ComputeRange(RendererPtr renderer, QObject *parent = nullptr)
        : RenderTask(std::move(renderer), parent)
    {
    }

    void setImage(QOpenGLTexture::Target target, GLuint textureId,
        const TextureData &texture, int level, int layer, int face)
    {
    }

Q_SIGNALS:
    void rangeComputed(const Range &range);

private:
    void render() override { }
    void finish() override { }
    void release() override { }
};

#endif // !defined(OPENGL_ENABLED)
