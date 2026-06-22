#include "GLTextureEditorItem.h"
#include "GLTexture.h"
#include "GLWindow.h"

#include <QMatrix4x4>
#include <QOpenGLShaderProgram>
#include <algorithm>
#include <cstdint>
#include <map>

class GLTextureEditorItem::ProgramCache
{
public:
    QOpenGLShaderProgram *getProgram(const ShaderDesc &desc)
    {
        auto &program = mPrograms[desc];
        if (!program.isLinked()) {
            program.create();
            auto vertexShader =
                new QOpenGLShader(QOpenGLShader::Vertex, &program);
            auto fragmentShader =
                new QOpenGLShader(QOpenGLShader::Fragment, &program);
            vertexShader->compileSourceCode(vertexShaderSource);
            fragmentShader->compileSourceCode(buildFragmentShader(desc));
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
    Q_ASSERT(!mImageTextureId && !mSharedTextureId);
}

void GLTextureEditorItem::releaseGpu()
{
    if (window().initialized()) {
        auto &gl = window().gl();
        if (mImageTextureId)
            gl.glDeleteTextures(1, &mImageTextureId);

        if (mPickerTexture.isCreated())
            mPickerTexture.destroy();
    }
    mImageTextureId = GL_NONE;
    mSharedTextureId = GL_NONE;

    mProgramCache.reset();
    mProgramCache = std::make_unique<ProgramCache>();
}

void GLTextureEditorItem::paintGpu(const QSizeF &bounds, const QPointF &offset)
{
    Q_ASSERT(glGetError() == GL_NO_ERROR);

    if (uploadTexture())
        renderTexture(getTransform(bounds, offset));

    Q_ASSERT(glGetError() == GL_NO_ERROR);
}

bool GLTextureEditorItem::downloadImage(TextureData *image)
{
    Q_ASSERT(image);
    if (!mSharedTextureId)
        return TextureEditorItem::downloadImage(image);

    window().makeCurrent();

    auto data = mImage;
    auto &gl = window().gl();
    const auto target = data.getTarget(mTextureSamples);

    if (!gl.glIsTexture(mSharedTextureId))
        return false;

    if (!GLTexture::download(gl, data, target, mSharedTextureId))
        return false;

    gl.glFinish();
    *image = std::move(data);
    return true;
}

void GLTextureEditorItem::copySharedTexture(ShareHandle textureHandle,
    int samples)
{
    Q_ASSERT(!textureHandle
        || textureHandle.type == ShareHandleType::OPENGL_TEXTURE_ID);

    mSharedTextureId = GL_NONE;
    if (textureHandle.type != ShareHandleType::OPENGL_TEXTURE_ID
        || !textureHandle) {
        update();
        return;
    }

    mSharedTextureId = static_cast<GLuint>(
        reinterpret_cast<std::uintptr_t>(textureHandle.handle));
    mTextureSamples = std::max(samples, 1);
    update();
}

GLWindow &GLTextureEditorItem::window()
{
    return *static_cast<GLWindow *>(parent());
}

bool GLTextureEditorItem::uploadTexture()
{
    if (mImage.isNull())
        return false;

    if (!mSharedTextureId && std::exchange(mUpload, false)) {
        auto &gl = window().gl();
        gl.glDeleteTextures(1, &mImageTextureId);
        mImageTextureId = GL_NONE;

        const auto result = GLTexture::upload(gl, mImage, mImage.getTarget(), 1,
            &mImageTextureId);
        Q_ASSERT(result);
    } else if (mSharedTextureId) {
        mUpload = false;
    }
    return (mSharedTextureId || mImageTextureId);
}

bool GLTextureEditorItem::renderTexture(const QMatrix4x4 &transform)
{
    Q_ASSERT(glGetError() == GL_NO_ERROR);
    auto &gl = window().gl();

    if (mSharedTextureId && !gl.glIsTexture(mSharedTextureId))
        mSharedTextureId = GL_NONE;

    const auto target =
        mImage.getTarget(mSharedTextureId ? mTextureSamples : 0);
    gl.glActiveTexture(GL_TEXTURE0);
    gl.glBindTexture(target,
        mSharedTextureId ? mSharedTextureId : mImageTextureId);

    gl.glEnable(GL_BLEND);
    gl.glBlendEquation(GL_FUNC_ADD);
    gl.glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    if (target != Texture::Target::Target2DMultisample
        && target != Texture::Target::Target2DMultisampleArray) {

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

    const auto desc = ShaderDesc{
        .target = target,
        .format = mImage.format(),
        .picker = mPickerEnabled,
    };
    auto *program = mProgramCache->getProgram(desc);
    if (!program)
        return false;

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
        program->setUniformValue("uPickerFragCoord", params.pickerFragCoord[0],
            params.pickerFragCoord[1]);
    }
    gl.glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);

    program->release();

    gl.glBindTexture(target, 0);
    if (mSharedTextureId)
        gl.glFinish();
    Q_ASSERT(glGetError() == GL_NO_ERROR);

    auto pickerColor = QVector4D{};
    if (mPickerEnabled) {
        gl.glBindTexture(mPickerTexture.target(), mPickerTexture.textureId());
        gl.glGetTexImage(mPickerTexture.target(), 0, GL_RGBA, GL_FLOAT,
            &pickerColor);
    }
    Q_EMIT pickerColorChanged(pickerColor);

    return true;
}
