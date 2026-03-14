#include "D3DStream.h"
#include "scripting/ScriptEngine.h"

D3DStream::D3DStream(const Stream &stream) : mItemId(stream.id)
{
    auto attributeIndex = 0;
    for (const auto *item : stream.items)
        if (auto attribute = castItem<Attribute>(item))
            mAttributes[attributeIndex++] = D3DAttribute{
                { attribute->id },
                attribute->name,
                attribute->normalize,
                attribute->divisor,
            };
}

void D3DStream::setAttribute(int attributeIndex, const Field &field,
    D3DBuffer *buffer, ScriptEngine &scriptEngine)
{
    const auto &block = *castItem<Block>(field.parent);
    const auto blockOffset = scriptEngine.evaluateValue(block.offset, block.id);
    auto &attribute = mAttributes[attributeIndex];
    attribute.usedItems += field.id;
    attribute.usedItems += block.id;
    attribute.usedItems += block.parent->id;
    attribute.buffer = buffer;
    attribute.type = field.dataType;
    attribute.count = field.count;
    attribute.stride = getBlockStride(block);
    attribute.offset = blockOffset + getFieldRowOffset(field);

    if (attribute.divisor == 0) {
        const auto rowCount =
            scriptEngine.evaluateValue(block.rowCount, block.id);
        if (mMaxElementCount < 0 || rowCount < mMaxElementCount)
            mMaxElementCount = rowCount;
    }

    if (!validateAttribute(attribute)) {
        mMessages +=
            MessageList::insert(field.id, MessageType::InvalidAttribute);
        mAttributes.remove(attributeIndex);
    }
}

bool D3DStream::validateAttribute(const D3DAttribute &attribute) const
{
    switch (attribute.count) {
    case 1:
    case 2:
    case 3:
    case 4:  return true;
    default: return false;
    }
}

bool D3DStream::usesBuffer(const D3DBuffer *buffer) const
{
    for (const auto &attribute : mAttributes)
        if (attribute.buffer == buffer)
            return true;
    return false;
}

auto D3DStream::findAttribute(const QString &semanticName, int semanticIndex)
    -> const D3DAttribute *
{
    for (auto &attribute : mAttributes)
        if ((attribute.name == semanticName + QString::number(semanticIndex))
            || (semanticIndex == 0 && attribute.name == semanticName)) {

            attribute.isUsed = true;
            return &attribute;
        }
    return nullptr;
}

void D3DStream::bind(D3DContext &context)
{
    auto buffers = std::vector<D3D12_VERTEX_BUFFER_VIEW>();
    for (const auto &attribute : mAttributes) {
        if (!attribute.isUsed)
            continue;

        attribute.buffer->prepareVertexBuffer(context);

        buffers.push_back(D3D12_VERTEX_BUFFER_VIEW{
            .BufferLocation = attribute.buffer->getDeviceAddress(),
            .SizeInBytes = static_cast<UINT>(attribute.buffer->size()),
            .StrideInBytes = static_cast<UINT>(attribute.stride),
        });
    }
    context.graphicsCommandList->IASetVertexBuffers(0,
        static_cast<UINT>(buffers.size()), buffers.data());
}
