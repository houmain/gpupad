
#include "TextureBase.h"
#include "EvaluatedPropertyCache.h"
#include "FileCache.h"
#include "FileDialog.h"
#include "Singletons.h"
#include <cmath>

void transformClearColor(std::array<double, 4> &color,
    TextureSampleType sampleType)
{
    const auto srgbToLinear = [](auto value) {
        if (value <= 0.0404482362771082)
            return value / 12.92;
        return std::pow((value + 0.055) / 1.055, 2.4);
    };
    const auto multiplyRGBA = [&](auto factor) {
        for (auto &c : color)
            c *= factor;
    };

    switch (sampleType) {
    case TextureSampleType::Normalized_sRGB:
    case TextureSampleType::Float:
        for (auto i = 0u; i < 3; ++i)
            color[i] = srgbToLinear(color[i]);
        break;
    case TextureSampleType::Int8:   multiplyRGBA(0x7F); break;
    case TextureSampleType::Int16:  multiplyRGBA(0x7FFF); break;
    case TextureSampleType::Int32:  multiplyRGBA(0x7FFFFFFF); break;
    case TextureSampleType::Uint8:  multiplyRGBA(0xFF); break;
    case TextureSampleType::Uint16: multiplyRGBA(0xFFFF); break;
    case TextureSampleType::Uint32: multiplyRGBA(0xFFFFFFFF); break;
    case TextureSampleType::Uint_10_10_10_2:
        for (auto i = 0u; i < 3; ++i)
            color[i] *= 1024.0;
        color[3] *= 4.0;
        break;
    default: break;
    }
}

TextureBase::TextureBase(const Texture &texture, ScriptEngine &scriptEngine)
    : mItemId(texture.id)
    , mFileName(texture.fileName)
    , mFlipVertically(texture.flipVertically)
    , mTarget(texture.target)
    , mFormat(texture.format)
    , mSamples(texture.samples)
    , mKind(getKind(texture))
{
    Singletons::evaluatedPropertyCache().evaluateTextureProperties(texture,
        &mWidth, &mHeight, &mDepth, &mLayers, &scriptEngine);

    if (mKind.dimensions < 2)
        mHeight = 1;
    if (mKind.dimensions < 3)
        mDepth = 1;
    if (!mKind.array)
        mLayers = 1;

    mUsedItems += texture.id;
}

TextureBase::TextureBase(const Buffer &buffer, Texture::Format format,
    ScriptEngine &scriptEngine)
    : mItemId(buffer.id)
    , mTarget(Texture::Target::TargetBuffer)
    , mFormat(format)
    , mWidth(getBufferSize(buffer, scriptEngine, mMessages))
    , mHeight(1)
    , mDepth(1)
    , mLayers(1)
    , mSamples(1)
    , mKind()
{
    mUsedItems += buffer.id;
}

bool TextureBase::operator==(const TextureBase &rhs) const
{
    return std::tie(mMessages, mFileName, mFlipVertically, mTarget, mFormat,
               mWidth, mHeight, mDepth, mLayers, mSamples)
        == std::tie(rhs.mMessages, rhs.mFileName, rhs.mFlipVertically,
            rhs.mTarget, rhs.mFormat, rhs.mWidth, rhs.mHeight, rhs.mDepth,
            rhs.mLayers, rhs.mSamples);
}

bool TextureBase::swap(TextureBase &other)
{
    if (mTarget != other.mTarget || mFormat != other.mFormat
        || mWidth != other.mWidth || mHeight != other.mHeight
        || mDepth != other.mDepth || mLayers != other.mLayers
        || mSamples != other.mSamples)
        return false;

    std::swap(mData, other.mData);
    std::swap(mDataWritten, other.mDataWritten);
    std::swap(mSystemCopyModified, other.mSystemCopyModified);
    std::swap(mDeviceCopyModified, other.mDeviceCopyModified);
    std::swap(mMipmapsInvalidated, other.mMipmapsInvalidated);
    return true;
}

void TextureBase::reload(bool forWriting)
{
    auto fileData = TextureData{};
    if (!FileDialog::isEmptyOrUntitled(mFileName)) {
        if (!Singletons::fileCache().getTexture(mFileName, mFlipVertically,
                &fileData))
            mMessages += MessageList::insert(mItemId,
                MessageType::LoadingFileFailed, mFileName);

        const auto hasSameDimensions = [&](const TextureData &data) {
            return (mFormat == data.format() && mWidth == data.width()
                && mHeight == data.height() && mDepth == data.depth()
                && mLayers == data.layers());
        };

        // validate dimensions when writing
        if (!forWriting || hasSameDimensions(fileData)) {
            mSystemCopyModified |= !mData.isSharedWith(fileData);
            mData = fileData;
        }
    }

    if (mData.isNull()) {
        if (!mData.create(mTarget, mFormat, mWidth, mHeight, mDepth, mLayers,
                mSamples > 1 ? 1 : 0)) {
            mData.create(mTarget, Texture::Format::RGBA8_UNorm, 1, 1, 1, 1, 1);
            mMessages += MessageList::insert(mItemId,
                MessageType::CreatingTextureFailed);
        }
        mData.clear();
        mSystemCopyModified = true;
    }
}
