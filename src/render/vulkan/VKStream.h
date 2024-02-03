#pragma once

#include "VKProgram.h"
#include "VKBuffer.h"

class VKStream
{
public:
    struct VKAttribute
    {
        QSet<ItemId> usedItems;
        QString name;
        bool normalize;
        int divisor;
        VKBuffer *buffer;
        Field::DataType type;
        int count;
        int stride;
        int offset;
    };

    explicit VKStream(const Stream &stream);
    void setAttribute(int attributeIndex,
        const Field &column, VKBuffer *buffer,
        ScriptEngine& scriptEngine);
    
    const std::vector<VKBuffer*> &getBuffers();
    const KDGpu::VertexOptions &getVertexOptions();
    const QSet<ItemId> &usedItems() const { return mUsedItems; }

private:
    bool validateAttribute(const VKAttribute &attribute) const;
    void invalidateVertexOptions();
    void updateVertexOptions();

    MessagePtrSet mMessages;
    QSet<ItemId> mUsedItems;
    QMap<int, VKAttribute> mAttributes;
    std::vector<VKBuffer*> mBuffers;
    KDGpu::VertexOptions mVertexOptions;
};
