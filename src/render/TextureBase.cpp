
#include "TextureBase.h"
#include "FileCache.h"
#include "FileDialog.h"
#include "Singletons.h"
#include "RenderSessionBase.h"
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

TextureBase::TextureBase(const Texture &texture,
    RenderSessionBase &renderSession)
    : mItemId(texture.id)
    , mFileName(texture.fileName)
    , mFlipVertically(texture.flipVertically)
    , mTarget(texture.target)
    , mFormat(texture.format)
    , mSamples(texture.samples)
    , mKind(getKind(texture))
{
    renderSession.evaluateTextureProperties(texture, &mWidth, &mHeight, &mDepth,
        &mLayers);

    if (mWidth <= 0)
        mWidth = 1;
    if (mHeight <= 0 || mKind.dimensions < 2)
        mHeight = 1;
    if (mDepth <= 0 || mKind.dimensions < 3)
        mDepth = 1;
    if (mKind.cubeMap)
        mHeight = mWidth;
    if (mLayers <= 0 || !mKind.array)
        mLayers = 1;
    if (mSamples <= 0)
        mSamples = 1;

    mUsedItems += texture.id;
}

TextureBase::TextureBase(const Buffer &buffer, Texture::Format format,
    RenderSessionBase &renderSession)
    : mItemId(buffer.id)
    , mTarget(Texture::Target::TargetBuffer)
    , mFormat(format)
    , mWidth(renderSession.getBufferSize(buffer))
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
    const auto properties = [](const TextureBase &a) {
        return std::tie(a.mMessages, a.mFileName, a.mFlipVertically, a.mTarget,
            a.mFormat, a.mWidth, a.mHeight, a.mDepth, a.mLayers, a.mSamples);
    };
    return properties(*this) == properties(rhs);
}

bool TextureBase::swap(TextureBase &other)
{
    if (mTarget != other.mTarget || mFormat != other.mFormat
        || mWidth != other.mWidth || mHeight != other.mHeight
        || mDepth != other.mDepth || mLayers != other.mLayers
        || mSamples != other.mSamples)
        return false;

    std::swap(mMipmapsInvalidated, other.mMipmapsInvalidated);

    mDeviceCopyModified = true;
    other.mDeviceCopyModified = true;
    return true;
}

void TextureBase::reload(bool forWriting)
{
    auto fileData = TextureData{};
    if (Singletons::fileCache().getTexture(mFileName, mFlipVertically,
            &fileData)) {
        // check if cache still matches the file before conversion
        if (!mFileData.isSharedWith(fileData)) {
            mFileData = fileData;
            fileData =
                fileData.convert(mFormat, mWidth, mHeight, mDepth, mLayers);
            if (!fileData.isNull()) {
                if (!mData.isSharedWith(fileData)) {
                    mSystemCopyModified = true;
                    mData = fileData;
                }
            } else if (!forWriting) {
                mMessages += MessageList::insert(mItemId,
                    MessageType::ConvertingFileFailed, mFileName);
            }
        }
    } else if (!FileDialog::isEmptyOrUntitled(mFileName)) {
        mMessages += MessageList::insert(mItemId,
            MessageType::LoadingFileFailed, mFileName);
    }

    if (mData.isNull()) {
        if (!mData.create(mTarget, mFormat, mWidth, mHeight, mDepth, mLayers,
                mSamples > 1 ? 1 : 0)) {
            mData.create(mTarget, Texture::Format::RGBA8_UNorm, 1, 1, 1, 1, 1);
            mMessages += MessageList::insert(mItemId,
                MessageType::CreatingTextureFailed);
        }
        mData.clear();
        mData.setFlippedVertically(mFlipVertically);
        mSystemCopyModified = true;
    }
}
