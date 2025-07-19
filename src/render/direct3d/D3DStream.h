#pragma once

#include "D3DBuffer.h"

class D3DStream
{
public:
    struct D3DAttribute
    {
        QString name;
        bool normalize{};
        int divisor{};
        D3DBuffer *buffer{};
        Field::DataType type{};
        int count{};
        int stride{};
        int offset{};
    };

    explicit D3DStream(const Stream &stream);
    void setAttribute(int attributeIndex, const Field &column,
        D3DBuffer *buffer, ScriptEngine &scriptEngine);
    const D3DAttribute *findAttribute(const QString &name) const;
    void bind(D3DContext &context);
    const QSet<ItemId> &usedItems() const { return mUsedItems; }
    int maxElementCount() const { return mMaxElementCount; }

private:
    bool validateAttribute(const D3DAttribute &attribute) const;

    ItemId mItemId{};
    MessagePtrSet mMessages;
    QSet<ItemId> mUsedItems;
    QMap<int, D3DAttribute> mAttributes;
    std::vector<D3D12_INPUT_ELEMENT_DESC> mInputLayout;
    int mMaxElementCount{ -1 };
};
