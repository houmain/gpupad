#include "GLTextureEditorItem.h"
#include "GLTexture.h"
#include "GLWindow.h"

#include <QMatrix4x4>
#include <QOpenGLShaderProgram>
#include <QScopeGuard>
#include <algorithm>
#include <cstdint>
#include <map>

class GLTextureEditorItem::ProgramCache
{
public:
    QOpenGLShaderProgram *getProgram(const ShaderDesc &desc,
        const QString &vertexShaderSource, const QString &fragmentShaderSource)
    {
        auto &program = mPrograms[desc];
        if (!program.isLinked()) {
            program.create();
            auto vertexShader =
                new QOpenGLShader(QOpenGLShader::Vertex, &program);
            auto fragmentShader =
                new QOpenGLShader(QOpenGLShader::Fragment, &program);
            vertexShader->compileSourceCode(vertexShaderSource);
            fragmentShader->compileSourceCode(fragmentShaderSource);
            program.addShader(vertexShader);
            program.addShader(fragmentShader);
            program.link();
        }
        return &program;
    }

private:
    std::map<ShaderDesc, QOpenGLShaderProgram> mPrograms;
};

GLTextureEditorItem::GLTextureEditorItem(GLWindow *parent)
    : TextureEditorItem(parent)
    , mProgramCache(new ProgramCache())
{
}

GLTextureEditorItem::~GLTextureEditorItem()
{
    Q_ASSERT(!mImageTextureId && !mPreviewTextureId);
}

void GLTextureEditorItem::releaseGpu()
{
    if (window().initialized()) {
        auto &gl = window().gl();
        if (mImageTextureId)
            gl.glDeleteTextures(1, &mImageTextureId);

        if (mPickerTexture.isCreated())
            mPickerTexture.destroy();
        if (mHistogramTexture.isCreated())
            mHistogramTexture.destroy();
    }
    mImageTextureId = GL_NONE;
    mPreviewTextureId = GL_NONE;
    mShareSync.reset();

    mProgramCache.reset();
    mProgramCache = std::make_unique<ProgramCache>();
}

void GLTextureEditorItem::paintGpu(const QMatrix4x4 &transform)
{
    Q_ASSERT(glGetError() == GL_NO_ERROR);

    if (updateTexture())
        renderTexture(transform);

    Q_ASSERT(glGetError() == GL_NO_ERROR);
}

bool GLTextureEditorItem::downloadImage(TextureData *image)
{
    Q_ASSERT(image);
    if (!mPreviewTextureId)
        return TextureEditorItem::downloadImage(image);

    if (!window().makeCurrent())
        return false;

    auto data = mImage;
    auto &gl = window().gl();
    const auto target = data.getTarget(mTextureSamples);
    if (mShareSync)
        mShareSync->beginUsage();
    const auto cleanup = qScopeGuard([&] {
        if (mShareSync)
            mShareSync->endUsage();
    });

    if (!gl.glIsTexture(mPreviewTextureId))
        return false;

    if (!GLTexture::download(gl, data, target, mPreviewTextureId))
        return false;

    *image = std::move(data);
    return true;
}

void GLTextureEditorItem::setPreviewTexture(ShareSyncPtr shareSync,
    ShareHandle textureHandle, int samples)
{
    Q_ASSERT(!textureHandle
        || textureHandle.type == ShareHandleType::OPENGL_TEXTURE_ID);

    mShareSync.reset();
    mPreviewTextureId = GL_NONE;
    if (textureHandle.type != ShareHandleType::OPENGL_TEXTURE_ID
        || !textureHandle) {
        render();
        return;
    }

    mShareSync = std::move(shareSync);
    mPreviewTextureId = static_cast<GLuint>(
        reinterpret_cast<std::uintptr_t>(textureHandle.handle));
    mTextureSamples = std::max(samples, 1);
    render();
}

GLWindow &GLTextureEditorItem::window()
{
    return static_cast<GLWindow &>(TextureEditorItem::window());
}

void GLTextureEditorItem::imageChanged()
{
    mShareSync.reset();
    mPreviewTextureId = GL_NONE;
}

bool GLTextureEditorItem::updateTexture()
{
    if (mImage.isNull())
        return false;

    if (!mPreviewTextureId && std::exchange(mUpload, false)) {
        auto &gl = window().gl();
        gl.glDeleteTextures(1, &mImageTextureId);
        mImageTextureId = GL_NONE;

        const auto result = GLTexture::upload(gl, mImage, mImage.getTarget(), 1,
            &mImageTextureId);
        Q_ASSERT(result);
    } else if (mPreviewTextureId) {
        mUpload = false;
    }
    return (mPreviewTextureId || mImageTextureId);
}

bool GLTextureEditorItem::renderTexture(const QMatrix4x4 &transform)
{
    Q_ASSERT(glGetError() == GL_NO_ERROR);
    auto &gl = window().gl();

    if (mPreviewTextureId && !gl.glIsTexture(mPreviewTextureId))
        mPreviewTextureId = GL_NONE;

    const auto target =
        mImage.getTarget(mPreviewTextureId ? mTextureSamples : 0);
    gl.glActiveTexture(GL_TEXTURE0);
    const auto usingPreviewTexture = (mPreviewTextureId != GL_NONE);
    if (usingPreviewTexture && mShareSync)
        mShareSync->beginUsage();

    if (mPreviewTextureId)
        gl.glBindTexture(target, mPreviewTextureId);
    else
        gl.glBindTexture(target, mImageTextureId);

    gl.glEnable(GL_BLEND);
    gl.glBlendEquation(GL_FUNC_ADD);
    gl.glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    if (target != QOpenGLTexture::Target2DMultisample
        && target != QOpenGLTexture::Target2DMultisampleArray) {

        gl.glTexParameteri(target, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        gl.glTexParameteri(target, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        gl.glTexParameteri(target, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);
        if (mMagnifyLinear) {
            gl.glTexParameteri(target, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
            gl.glTexParameteri(target, GL_TEXTURE_MIN_FILTER,
                GL_LINEAR_MIPMAP_LINEAR);
        } else {
            gl.glTexParameteri(target, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
            gl.glTexParameteri(target, GL_TEXTURE_MIN_FILTER,
                GL_NEAREST_MIPMAP_NEAREST);
        }
    }

    const auto desc = ShaderDesc{ target, mImage.format(), mPickerEnabled,
        mHistogramEnabled };
    if (auto *program = mProgramCache->getProgram(desc, vertexShaderSource,
            buildFragmentShader(desc))) {
        const auto params = getParams(transform, mTextureSamples);

        program->bind();
        program->setUniformValue("uTexture", 0);
        program->setUniformValue("uTransform", transform);
        program->setUniformValue("uSize", params.width, params.height);
        program->setUniformValue("uLevel", params.level);
        program->setUniformValue("uFace", params.face);
        program->setUniformValue("uLayer", params.layer);
        program->setUniformValue("uSample", params.sample);
        program->setUniformValue("uSamples", params.samples);
        program->setUniformValue("uFlipVertically", params.flipVertically);
        program->setUniformValue("uMappingOffset", params.mappingOffset);
        program->setUniformValue("uMappingFactor", params.mappingFactor);
        program->setUniformValue("uColorMask",
            static_cast<GLint>(params.colorMask));

        if (mPickerEnabled) {
            if (!mPickerTexture.isCreated()) {
                mPickerTexture.setSize(1, 1);
                mPickerTexture.setFormat(QOpenGLTexture::RGBA32F);
                mPickerTexture.allocateStorage();
            }
            gl.glBindImageTexture(1, mPickerTexture.textureId(), 0, GL_FALSE, 0,
                GL_WRITE_ONLY, GL_RGBA32F);
            program->setUniformValue("uPickerColor", 1);
            program->setUniformValue("uPickerFragCoord",
                mMousePosition + QPointF(0.5, 0.5));
        }
        if (mHistogramEnabled) {
            if (!mHistogramTexture.isCreated()
                || mHistogramTexture.width() != mHistogramBins.size()) {
                mHistogramTexture.destroy();
                mHistogramTexture.setSize(mHistogramBins.size());
                mHistogramTexture.setFormat(QOpenGLTexture::R32U);
                mHistogramTexture.allocateStorage();
            }
            gl.glBindImageTexture(2, mHistogramTexture.textureId(), 0, GL_FALSE,
                0, GL_READ_WRITE, GL_R32UI);
            program->setUniformValue("uHistogram", 2);
            program->setUniformValue("uHistogramOffset",
                static_cast<float>(-mHistogramBounds.minimum));
            const auto scaleToBins = mHistogramBins.size() / 3 - 1;
            program->setUniformValue("uHistogramFactor",
                static_cast<float>(1 / mHistogramBounds.range() * scaleToBins));
        }

        gl.glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);

        gl.glMemoryBarrier(GL_TEXTURE_UPDATE_BARRIER_BIT);

        auto pickerColor = QVector4D{};
        if (mPickerEnabled) {
            gl.glBindTexture(mPickerTexture.target(),
                mPickerTexture.textureId());
            gl.glGetTexImage(mPickerTexture.target(), 0, GL_RGBA, GL_FLOAT,
                &pickerColor);
        }
        Q_EMIT pickerColorChanged(pickerColor);

        if (mHistogramEnabled) {
            gl.glBindTexture(mHistogramTexture.target(),
                mHistogramTexture.textureId());
            gl.glGetTexImage(mHistogramTexture.target(), 0, GL_RED_INTEGER,
                GL_UNSIGNED_INT, mHistogramBins.data());
            updateHistogram();
        }

        program->release();
    }

    gl.glBindTexture(target, 0);
    if (usingPreviewTexture && mShareSync)
        mShareSync->endUsage();
    Q_ASSERT(glGetError() == GL_NO_ERROR);
    return true;
}
