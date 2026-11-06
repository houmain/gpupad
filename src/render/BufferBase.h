#pragma once

#include "message/MessageList.h"
#include "session/Item.h"
#include <cstdint>

class BufferBase
{
public:
    bool operator==(const BufferBase &rhs) const;
    void updateUntitledFilename(const BufferBase &rhs);

    ItemId itemId() const { return mItemId; }
    const QByteArray &data() const { return mData; }
    QByteArray &writableData();
    const QString &fileName() const { return mFileName; }
    const QSet<ItemId> &usedItems() const { return mUsedItems; }
    int size() const { return mSize; }
    uint64_t contentRevision() const { return mContentRevision; }
    void markDeviceWrite();

protected:
    explicit BufferBase(int size);
    BufferBase(const Buffer &buffer, int size);
    bool swap(BufferBase &other);

    MessageList mMessages;
    ItemId mItemId{};
    QString mFileName;
    int mSize{};
    QByteArray mData;
    QSet<ItemId> mUsedItems;
    bool mSystemCopyModified{};
    bool mDeviceCopyModified{};
    uint64_t mContentRevision{};
};
