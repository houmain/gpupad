#pragma once

#include "Range.h"
#include "TextureData.h"
#include <QOpenGLTexture>
#include <QOpenGLShaderProgram>
#include <QOpenGLBuffer>

class GLComputeRange
{
public:
    void render();
    void release();

    void setImage(GLuint textureId, const TextureData &texture, int level, int layer, int face);
    const Range &range() const { return mRange; }

private:
    using ProgramKey = std::tuple<QOpenGLTexture::Target, QOpenGLTexture::TextureFormat>;

    QOpenGLShaderProgram *getProgram(QOpenGLTexture::Target target,
                                     QOpenGLTexture::TextureFormat format);

    QOpenGLTexture::Target mTarget{ };
    QOpenGLTexture::TextureFormat mFormat{ };
    QSize mSize{ };
    int mLevel{ };
    int mLayer{ };
    int mFace{ };
    GLuint mTextureId{ };
    Range mRange{ 0, 1 };
    QOpenGLBuffer mBuffer;
    std::map<ProgramKey, QOpenGLShaderProgram> mPrograms;
};
