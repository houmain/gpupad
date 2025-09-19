
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
    , mIsSequence(texture.isSequence)
    , mSequencePattern(texture.sequencePattern)
    , mFrameStart(texture.frameStart)
    , mFrameEnd(texture.frameEnd)
    , mLoopSequence(texture.loopSequence)
    , mCurrentFrame(texture.currentFrame)
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

    // Debug: Show sequence mode initialization
    if (mIsSequence) {
        mMessages += MessageList::insert(mItemId, MessageType::ScriptMessage,
            QString("Sequence initialized: pattern='%1', frames %2-%3, loop=%4")
                .arg(mSequencePattern).arg(mFrameStart).arg(mFrameEnd).arg(mLoopSequence ? "on" : "off"));
    }
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
    // Update sequence frame before loading
    updateSequenceFrame();

    auto fileData = TextureData{};
    QString actualFileName = resolveCurrentFileName();

    // Debug: Show sequence mode status
    if (mIsSequence) {
        mMessages += MessageList::insert(mItemId, MessageType::ScriptMessage,
            QString("Sequence mode: frame %1/%2 -> %3").arg(mCurrentFrame).arg(mFrameEnd).arg(actualFileName));
    } else if (!mFileName.isEmpty()) {
        mMessages += MessageList::insert(mItemId, MessageType::ScriptMessage,
            QString("Single file mode: %1").arg(actualFileName));
    }

    if (Singletons::fileCache().getTexture(actualFileName, mFlipVertically,
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
                    MessageType::ConvertingFileFailed, actualFileName);
            }
        }
    } else if (!FileDialog::isEmptyOrUntitled(actualFileName)) {
        mMessages += MessageList::insert(mItemId,
            MessageType::LoadingFileFailed, actualFileName);
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

QString TextureBase::resolveCurrentFileName() const
{
    if (!mIsSequence)
        return mFileName;

    return Singletons::fileCache().buildSequenceFileName(
        mFileName, mSequencePattern, mCurrentFrame);
}

void TextureBase::updateSequenceFrame()
{
    if (!mIsSequence)
        return;

    // Advance frame
    mCurrentFrame++;

    if (mCurrentFrame > mFrameEnd) {
        if (mLoopSequence) {
            mCurrentFrame = mFrameStart;
            mMessages += MessageList::insert(mItemId, MessageType::ScriptMessage,
                QString("Sequence looped: back to frame %1").arg(mCurrentFrame));
        } else {
            mCurrentFrame = mFrameEnd;  // Clamp to last frame
            mMessages += MessageList::insert(mItemId, MessageType::ScriptMessage,
                QString("Sequence ended: clamped to frame %1").arg(mCurrentFrame));
        }
    }
}
