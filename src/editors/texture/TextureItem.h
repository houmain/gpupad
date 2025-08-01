#pragma once

#include "Range.h"
#include "TextureData.h"
#include "render/ShareSync.h"
#include <QObject>
#include <QOpenGLTexture>

class GLWidget;
class ComputeRange;

class TextureItem final : public QObject
{
    Q_OBJECT
public:
    explicit TextureItem(GLWidget *parent);
    ~TextureItem() override;
    void releaseGL();
    void paintGL(const QMatrix4x4 &transform);
    void setImage(TextureData image);
    const TextureData &image() const { return mImage; }
    void setPreviewTexture(ShareSyncPtr sync, GLuint textureId, int samples);
    void setPreviewTexture(ShareSyncPtr sync, ShareHandle handle,
        int samples);

    bool canFilter() const;
    void setMagnifyLinear(bool magnifyLinear)
    {
        mMagnifyLinear = magnifyLinear;
        update();
    }
    bool magnifyLinear() const { return mMagnifyLinear; }
    void setLevel(float level)
    {
        mLevel = level;
        update();
    }
    float level() const { return mLevel; }
    void setFace(int face)
    {
        mFace = face;
        update();
    }
    int face() const { return mFace; }
    void setLayer(float layer)
    {
        mLayer = layer;
        update();
    }
    float layer() const { return mLayer; }
    void setSample(int sample)
    {
        mSample = sample;
        update();
    }
    int sample() const { return mSample; }
    void setFlipVertically(bool flip)
    {
        mFlipVertically = flip;
        update();
    }
    bool flipVertically() const { return mFlipVertically; }
    void setPickerEnabled(bool enabled) { mPickerEnabled = enabled; }
    bool pickerEnabled() const { return mPickerEnabled; }
    void setMappingRange(const Range &bounds);
    const Range &mappingRange() const { return mMappingRange; }
    void setHistogramBinCount(int count);
    void setHistogramBounds(const Range &range);
    const Range &histogramBounds() const { return mHistogramBounds; }
    void computeHistogramBounds();
    void setHistogramEnabled(bool enabled) { mHistogramEnabled = enabled; }
    bool histogramEnabled() const { return mHistogramEnabled; }
    void setColorMask(unsigned int colorMask);
    unsigned int colorMask() const { return mColorMask; }
    QRectF boundingRect() const { return mBoundingRect; }
    void setMousePosition(const QPointF &mousePosition);

Q_SIGNALS:
    void pickerColorChanged(const QVector4D &color);
    void histogramChanged(const QVector<qreal> &histogram);
    void histogramBoundsComputed(const Range &range);

private:
    class ProgramCache;

    GLWidget &widget();
    void update();
    bool updateTexture();
    bool renderTexture(const QMatrix4x4 &transform);
    void updateHistogram();

    std::unique_ptr<ProgramCache> mProgramCache;
    QRect mBoundingRect;
    TextureData mImage;
    GLuint mImageTextureId{};
    ShareSyncPtr mShareSync;
    int mPreviewSamples{ 1 };
    GLuint mPreviewTextureId{};
    GLuint mSharedTextureId{};
    void *mSharedTextureHandle{};
    bool mMagnifyLinear{};
    float mLevel{};
    int mFace{};
    float mLayer{};
    int mSample{ -1 };
    bool mFlipVertically{};
    bool mPickerEnabled{};
    QOpenGLTexture mPickerTexture{ QOpenGLTexture::Target1D };
    bool mHistogramEnabled{};
    QOpenGLTexture mHistogramTexture{ QOpenGLTexture::Target1D };
    Range mMappingRange{ 0, 1 };
    Range mHistogramBounds{ 0, 1 };
    QVector<quint32> mHistogramBins;
    QVector<quint32> mPrevHistogramBins;
    QPointF mMousePosition{};
    bool mUpload{};
    ComputeRange *mComputeRange{};
    unsigned int mColorMask{};
};
