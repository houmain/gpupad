#pragma once

#include "MessageList.h"
#include "session/Item.h"

class BufferBase
{
public:
    bool operator==(const BufferBase &rhs) const;
    void updateUntitledFilename(const BufferBase &rhs);

    ItemId itemId() const { return mItemId; }
    const QByteArray &data() const { return mData; }
    const QString &fileName() const { return mFileName; }
    const QSet<ItemId> &usedItems() const { return mUsedItems; }
    int size() const { return mSize; }

protected:
    explicit BufferBase(int size);
    BufferBase(const Buffer &buffer, int size);

    MessagePtrSet mMessages;
    ItemId mItemId{};
    QString mFileName;
    int mSize{};
    QByteArray mData;
    QSet<ItemId> mUsedItems;
    bool mSystemCopyModified{};
    bool mDeviceCopyModified{};
};
