#pragma once

#include "MessageList.h"
#include "TextureData.h"
#include "session/Item.h"
#include "render/ShareHandle.h"

class RenderSessionBase;

class TextureBase
{
public:
    TextureBase(const Texture &texture, RenderSessionBase &renderSession);
    TextureBase(const Buffer &buffer, Texture::Format format,
        RenderSessionBase &renderSession);
    TextureBase(TextureData data, int samples);
    virtual ~TextureBase() = default;
    bool operator==(const TextureBase &rhs) const;

    ItemId itemId() const { return mItemId; }
    const QString &fileName() const { return mFileName; }
    TextureKind kind() const { return mKind; }
    Texture::Target target() const { return mTarget; }
    int width() const { return mWidth; }
    int height() const { return mHeight; }
    int depth() const { return mDepth; }
    int samples() const { return mSamples; }
    int layers() const { return mLayers; }
    int levels() const { return (mSamples > 1 ? 1 : mData.levels()); }
    Texture::Format format() const { return mFormat; }
    const TextureData &data() const { return mData; }
    const QSet<ItemId> &usedItems() const { return mUsedItems; }
    bool deviceCopyModified() const { return mDeviceCopyModified; }
    ShareHandle shareHandle() const { return mShareHandle; }

protected:
    bool swap(TextureBase &other);
    void reload(bool forWriting);

    ItemId mItemId{ };
    MessagePtrSet mMessages;
    QString mFileName;
    TextureData::RowOrder mRowOrder{ };
    Texture::Target mTarget{ };
    Texture::Format mFormat{ };
    int mWidth{ };
    int mHeight{ };
    int mDepth{ };
    int mLayers{ };
    int mSamples{ };
    TextureData mData;
    QSet<ItemId> mUsedItems;
    TextureKind mKind{ };
    TextureData mFileData;
    bool mSystemCopyModified{ };
    bool mDeviceCopyModified{ };
    bool mMipmapsInvalidated{ };
    ShareHandleSource mShareHandle;
};

void transformClearColor(std::array<double, 4> &color,
    TextureSampleType sampleType);
