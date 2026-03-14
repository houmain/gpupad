#pragma once

#if defined(_WIN32)

#  include "D3DBuffer.h"

class D3DStream
{
public:
    struct D3DAttribute
    {
        QSet<ItemId> usedItems;
        QString name;
        bool normalize{};
        int divisor{};
        D3DBuffer *buffer{};
        Field::DataType type{};
        int count{};
        int stride{};
        int offset{};
        bool isUsed{};
    };

    explicit D3DStream(const Stream &stream);
    ItemId itemId() const { return mItemId; }
    void setAttribute(int attributeIndex, const Field &column,
        D3DBuffer *buffer, ScriptEngine &scriptEngine);
    bool usesBuffer(const D3DBuffer *buffer) const;
    const D3DAttribute *findAttribute(const QString &semanticName,
        int semanticIndex);
    void bind(D3DContext &context);
    int maxElementCount() const { return mMaxElementCount; }

private:
    bool validateAttribute(const D3DAttribute &attribute) const;

    ItemId mItemId{};
    MessagePtrSet mMessages;
    QMap<int, D3DAttribute> mAttributes;
    std::vector<D3D12_INPUT_ELEMENT_DESC> mInputLayout;
    int mMaxElementCount{ -1 };
};

#endif // _WIN32
