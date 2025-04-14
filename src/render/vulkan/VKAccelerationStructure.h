#pragma once

#include "VKItem.h"

class VKAccelerationStructure
{
public:
    VKAccelerationStructure(const AccelerationStructure &accelStruct);
    bool operator==(const VKAccelerationStructure &rhs) const;

    ItemId itemId() const { return mItemId; }
    const QSet<ItemId> &usedItems() const { return mUsedItems; }
    const KDGpu::AccelerationStructure &topLevelAs() const
    {
        return mTopLevelAs;
    }

    void prepare(VKContext &context);

private:
    ItemId mItemId{};
    MessagePtrSet mMessages;
    QSet<ItemId> mUsedItems;
    std::vector<KDGpu::AccelerationStructure> mBottomLevelAs;
    KDGpu::AccelerationStructure mTopLevelAs;
};
