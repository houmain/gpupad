#ifndef TEXTUREITEM_H
#define TEXTUREITEM_H

#include "TextureData.h"
#include <QGraphicsView>
#include <QOpenGLTexture>
#include <QGraphicsItem>
#include <QOpenGLShaderProgram>
#include <QOpenGLDebugLogger>
#include <QOpenGLFunctions_3_3_Core>
#include <map>

class ZeroCopyContext;

class TextureItem : public QGraphicsItem
{
public:
    TextureItem();
    TextureItem(const TextureItem&) = delete;
    TextureItem& operator=(const TextureItem&) = delete;
    ~TextureItem() override;
    void releaseGL();

    void setImage(TextureData image);
    void setPreviewTexture(QOpenGLTexture::Target target, GLuint textureId);
    GLuint resetTexture();
    void setMagnifyLinear(bool magnifyLinear);
    QRectF boundingRect() const override { return mBoundingRect; }
    void paint(QPainter *painter, const QStyleOptionGraphicsItem *, QWidget *) override;

private:
    ZeroCopyContext &context();
    bool updateTexture();
    bool renderTexture(const QMatrix &transform);

    QScopedPointer<ZeroCopyContext> mContext;
    QRect mBoundingRect;
    TextureData mImage;
    GLuint mImageTextureId{ };
    QOpenGLTexture::Target mPreviewTarget{ };
    GLuint mPreviewTextureId{ };
    bool mMagnifyLinear{ };
    bool mUpload{ };
};

#endif // TEXTUREITEM_H
