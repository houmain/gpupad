#pragma once

#include "Range.h"
#include "TextureData.h"
#include "render/ShareHandle.h"
#include <QObject>
#include <QPointF>
#include <QSizeF>
#include <QRect>
#include <QString>
#include <array>
#include <cstdint>

class QMatrix4x4;
class QWindow;

class TextureEditorItem : public QObject
{
    Q_OBJECT
public:
    enum class WrapMode {
        ClampToBorder,
        ClampToEdge,
        Repeat,
        MirroredRepeat,
    };

    explicit TextureEditorItem(QWindow *parent);
    ~TextureEditorItem() override;
    virtual void releaseGpu() = 0;
    virtual void prepareGpu() { }
    virtual void paintGpu(const QSizeF &bounds, const QPointF &offset) = 0;
    virtual void submittedGpu() { }
    void setImage(TextureData image);
    const TextureData &image() const { return mImage; }
    virtual bool downloadImage(TextureData *image);
    virtual void copySharedTexture(ShareHandle textureHandle, int samples) = 0;

    bool canFilter() const;
    void setMagnifyLinear(bool magnifyLinear)
    {
        mMagnifyLinear = magnifyLinear;
        update();
    }
    bool magnifyLinear() const { return mMagnifyLinear; }
    void setWrapMode(WrapMode wrapMode)
    {
        mWrapMode = wrapMode;
        update();
    }
    WrapMode wrapMode() const { return mWrapMode; }
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
    void setMappingRange(const Range &range);
    const Range &mappingRange() const { return mMappingRange; }
    void setMappingSelection(const Range &selection);
    const Range &mappingSelection() const { return mMappingSelection; }
    void setColorMask(unsigned int colorMask);
    unsigned int colorMask() const { return mColorMask; }
    QRectF boundingRect() const { return mBoundingRect; }
    void setMousePosition(const QPointF &mousePosition);

Q_SIGNALS:
    void pickerColorChanged(const QVector4D &color);

protected:
    struct ShaderDesc
    {
        Texture::Target target{};
        Texture::Format format{};
        bool picker{};

        friend bool operator<(const ShaderDesc &a, const ShaderDesc &b)
        {
            return std::tie(a.target, a.format, a.picker)
                < std::tie(b.target, b.format, b.picker);
        }
    };

    struct Params
    {
        std::array<float, 16> transform{};
        int32_t transformTexCoords{};
        int32_t flipVertically{};

        float width{};
        float height{};
        std::array<float, 2> pickerFragCoord{};
        float level{};
        float layer{};
        int32_t face{};
        int32_t sample{};
        int32_t samples{};
        float mappingOffset{};
        float mappingFactor{};
        uint32_t colorMask{};
    };
    static_assert(sizeof(Params) == 120);

    static QString vertexShaderSource;
    static QString buildFragmentShader(const ShaderDesc &desc);
    static bool canLinearFilter(Texture::Format format);

    QWindow &window();
    void update();
    QMatrix4x4 getTransform(const QSizeF &bounds, const QPointF &offset);
    bool transformTextureCoordinates() const;
    Params getParams(const QMatrix4x4 &transform, int textureSamples) const;

    QRect mBoundingRect;
    TextureData mImage;
    int mTextureSamples{ 1 };
    bool mMagnifyLinear{};
    WrapMode mWrapMode{};
    float mLevel{};
    int mFace{};
    float mLayer{};
    int mSample{ -1 };
    bool mFlipVertically{};
    bool mPickerEnabled{};
    Range mMappingRange{ 0, 1 };
    Range mMappingSelection{ 0, 1 };
    QPointF mMousePosition{};
    bool mUpload{};
    unsigned int mColorMask{};
};
