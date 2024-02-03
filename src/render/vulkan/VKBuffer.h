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

    void clear();
    void copy(VKBuffer &source);
    bool swap(VKBuffer &other);
    const KDGpu::Buffer &getReadOnlyBuffer(VKContext &context);
    const KDGpu::Buffer &getReadWriteBuffer(VKContext &context);
    bool download(bool checkModification);

private:
    void reload();
    void createBuffer(KDGpu::Device &device);
    void upload(VKContext &context);

    MessagePtrSet mMessages;
    ItemId mItemId{ };
    QString mFileName;
    int mSize{ };
    QByteArray mData;
    QSet<ItemId> mUsedItems;
    KDGpu::Buffer mBuffer;
    bool mSystemCopyModified{ };
    bool mDeviceCopyModified{ };
};

int getBufferSize(const Buffer &buffer,
    ScriptEngine &scriptEngine, MessagePtrSet &messages);
