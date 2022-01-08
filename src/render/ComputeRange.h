#pragma once

#include "RenderTask.h"
#include "Range.h"
#include <QOpenGLTexture>
#include <QOpenGLShaderProgram>
#include <QOpenGLBuffer>
#include "TextureData.h"

class ComputeRange final : public RenderTask
{
    Q_OBJECT
public:
    explicit ComputeRange(QObject *parent = nullptr);
    ~ComputeRange() override;

    void setImage(GLuint textureId, const TextureData &texture, int level, int layer, int face);

Q_SIGNALS:
     void rangeComputed(const Range &range);

private:
    using ProgramKey = std::tuple<QOpenGLTexture::Target, QOpenGLTexture::TextureFormat>;

    void render() override;
    void finish() override;
    void release() override;
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
