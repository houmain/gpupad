#include "VKStream.h"

VKStream::VKStream(const Stream &stream) : mItemId(stream.id)
{
    mUsedItems += stream.id;

    auto attributeIndex = 0;
    for (const auto *item : stream.items) {
        if (auto attribute = castItem<Attribute>(item)) {
            mUsedItems += item->id;
            mAttributes[attributeIndex] = VKAttribute{
                attribute->name,
                attribute->normalize,
                attribute->divisor,
            };
        }
        ++attributeIndex;
    }
}

void VKStream::setAttribute(int attributeIndex, const Field &field,
    VKBuffer *buffer, ScriptEngine &scriptEngine)
{
    const auto &block = *castItem<Block>(field.parent);
    const auto blockOffset =
        scriptEngine.evaluateValue(block.offset, block.id, mMessages);
    auto &attribute = mAttributes[attributeIndex];
    mUsedItems += field.id;
    mUsedItems += block.id;
    mUsedItems += block.parent->id;
    attribute.buffer = buffer;
    attribute.type = field.dataType;
    attribute.count = field.count;
    attribute.stride = getBlockStride(block);
    attribute.offset = blockOffset + getFieldRowOffset(field);

    if (attributeIndex == 0)
        mDefaultElementCount =
            scriptEngine.evaluateValue(block.rowCount, block.id, mMessages);

    if (!validateAttribute(attribute)) {
        mMessages +=
            MessageList::insert(field.id, MessageType::InvalidAttribute);
        mAttributes.remove(attributeIndex);
    }
    invalidateVertexOptions();
}

bool VKStream::validateAttribute(const VKAttribute &attribute) const
{
    switch (attribute.count) {
    case 1:
    case 2:
    case 3:
    case 4:  break;
    default: return false;
    }
    return true;
}

void VKStream::invalidateVertexOptions()
{
    mVertexOptions = {};
    mBuffers = {};
    mBufferOffsets = {};
}

void VKStream::updateVertexOptions()
{
    if (!mBuffers.empty())
        return;

    for (const auto &attribute : mAttributes)
        if (!validateAttribute(attribute)) {
            mMessages += MessageList::insert(mItemId,
                MessageType::AttributeNotSet, attribute.name);
            return;
        }

    auto location = 0;
    for (const auto &attribute : mAttributes) {
        const auto binding = static_cast<uint32_t>(mBuffers.size());

        mVertexOptions.buffers.push_back(KDGpu::VertexBufferLayout{
            .binding = binding,
            .stride = static_cast<uint32_t>(attribute.stride),
            .inputRate = (attribute.divisor ? KDGpu::VertexRate::Instance
                                            : KDGpu::VertexRate::Vertex),
        });
        mBuffers.push_back(attribute.buffer);
        mBufferOffsets.push_back(attribute.offset);

        mVertexOptions.attributes.push_back(KDGpu::VertexAttribute{
            .location = static_cast<uint32_t>(location++),
            .binding = binding,
            .format = toKDGpu(attribute.type, attribute.count),
            .offset = 0,
        });
    }
}

const std::vector<VKBuffer *> &VKStream::getBuffers()
{
    updateVertexOptions();
    return mBuffers;
}

const std::vector<int> &VKStream::getBufferOffsets()
{
    updateVertexOptions();
    return mBufferOffsets;
}

const KDGpu::VertexOptions &VKStream::getVertexOptions()
{
    updateVertexOptions();
    return mVertexOptions;
}
