#pragma once

#include "VKItem.h"
#include "scripting/ScriptEngine.h"

class VKBuffer
{
public:
    VKBuffer(const Buffer &buffer, ScriptEngine &scriptEngine);
    void updateUntitledFilename(const VKBuffer &rhs);
    bool operator==(const VKBuffer &rhs) const;

    ItemId itemId() const { return mItemId; }
    const QByteArray &data() const { return mData; }
    const QString &fileName() const { return mFileName; }
    const QSet<ItemId> &usedItems() const { return mUsedItems; }
    int size() const { return mSize; }

    void clear(VKContext &context);
    void copy(VKContext &context, VKBuffer &source);
    bool swap(VKBuffer &other);
    const KDGpu::Buffer &getReadOnlyBuffer(VKContext &context);
    const KDGpu::Buffer &getReadWriteBuffer(VKContext &context);
    bool download(VKContext &context, bool checkModification);

private:
    void reload();
    void createBuffer(KDGpu::Device &device);
    void upload(VKContext &context);

    MessagePtrSet mMessages;
    ItemId mItemId{};
    QString mFileName;
    int mSize{};
    QByteArray mData;
    QSet<ItemId> mUsedItems;
    KDGpu::Buffer mBuffer;
    bool mSystemCopyModified{};
    bool mDeviceCopyModified{};
};

int getBufferSize(const Buffer &buffer, ScriptEngine &scriptEngine,
    MessagePtrSet &messages);
bool downloadBuffer(VKContext &context, const KDGpu::Buffer &buffer,
    uint64_t size, std::function<void(const std::byte *)> &&callback);
