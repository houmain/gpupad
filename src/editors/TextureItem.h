#ifndef TEXTUREITEM_H
#define TEXTUREITEM_H

#include "TextureData.h"
#include <QObject>
#include <QGraphicsView>
#include <QOpenGLTexture>
#include <QGraphicsItem>
#include <QOpenGLShaderProgram>
#include <QOpenGLVertexArrayObject>

class ZeroCopyContext;

class TextureItem final : public QObject, public QGraphicsItem
{
    Q_OBJECT
    Q_INTERFACES(QGraphicsItem)
public:
    explicit TextureItem(QObject *parent = nullptr);
    ~TextureItem() override;
    void releaseGL();

    void setImage(TextureData image);
    void setPreviewTexture(QOpenGLTexture::Target target, GLuint textureId);
    GLuint resetTexture();
    void setMagnifyLinear(bool magnifyLinear) { mMagnifyLinear = magnifyLinear; update(); }
    bool magnifyLinear() const { return mMagnifyLinear; }
    void setLevel(float level) { mLevel = level; update(); }
    float level() const { return mLevel; }
    void setFace(int face) { mFace = face; update(); }
    int face() const { return mFace; }
    void setLayer(float layer) { mLayer = layer; update(); }
    float layer() const { return mLayer; }
    void setSample(int sample) { mSample = sample; update(); }
    int sample() const { return mSample; }
    void setFlipVertically(bool flip) { mFlipVertically = flip; update(); }
    bool flipVertically() const { return mFlipVertically; }
    QRectF boundingRect() const override { return mBoundingRect; }
    void paint(QPainter *painter, const QStyleOptionGraphicsItem *, QWidget *) override;

private:
    ZeroCopyContext &context();
    bool updateTexture();
    bool renderTexture(const QMatrix4x4 &transform);

    QOpenGLVertexArrayObject mVao;
    QScopedPointer<ZeroCopyContext> mContext;
    QRect mBoundingRect;
    TextureData mImage;
    GLuint mImageTextureId{ };
    QOpenGLTexture::Target mPreviewTarget{ };
    GLuint mPreviewTextureId{ };
    bool mMagnifyLinear{ };
    float mLevel{ };
    int mFace{ };
    float mLayer{ };
    int mSample{ -1 };
    int mSamples{ };
    bool mFlipVertically{ };
    bool mUpload{ };
};

#endif // TEXTUREITEM_H
