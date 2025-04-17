#pragma once

#include "VKBuffer.h"

class VKStream
{
public:
    explicit VKStream(const Stream &stream);
    void setAttribute(int attributeIndex, const Field &column, VKBuffer *buffer,
        ScriptEngine &scriptEngine);

    const std::vector<VKBuffer *> &getBuffers();
    const std::vector<int> &getBufferOffsets();
    const KDGpu::VertexOptions &getVertexOptions();
    const QSet<ItemId> &usedItems() const { return mUsedItems; }
    int maxElementCount() const { return mMaxElementCount; }

private:
    struct VKAttribute
    {
        QString name;
        bool normalize{};
        int divisor{};
        VKBuffer *buffer{};
        Field::DataType type{};
        int count{};
        int stride{};
        int offset{};
    };

    bool validateAttribute(const VKAttribute &attribute) const;
    void invalidateVertexOptions();
    void updateVertexOptions();

    ItemId mItemId{};
    MessagePtrSet mMessages;
    QSet<ItemId> mUsedItems;
    QMap<int, VKAttribute> mAttributes;
    std::vector<VKBuffer *> mBuffers;
    std::vector<int> mBufferOffsets;
    KDGpu::VertexOptions mVertexOptions;
    int mMaxElementCount{ -1 };
};
