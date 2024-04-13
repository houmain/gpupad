#include "VKStream.h"

VKStream::VKStream(const Stream &stream)
{
    mUsedItems += stream.id;

    auto attributeIndex = 0;
    for (const auto &item : stream.items) {
        if (auto attribute = castItem<Attribute>(item))
            mAttributes[attributeIndex] = VKAttribute{
                { item->id },
                attribute->name,
                attribute->normalize,
                attribute->divisor,
                nullptr, { }, 0, 0, 0 };
        ++attributeIndex;
    }
}

void VKStream::setAttribute(int attributeIndex, const Field &field, 
    VKBuffer *buffer, ScriptEngine& scriptEngine)
{
    const auto &block = *castItem<Block>(field.parent);
    const auto blockOffset = scriptEngine.evaluateValue(block.offset, block.id, mMessages);
    auto &attribute = mAttributes[attributeIndex];
    attribute.usedItems += field.id;
    attribute.buffer = buffer;
    attribute.type = field.dataType;
    attribute.count = field.count;
    if (auto block = castItem<Block>(field.parent)) {
        attribute.usedItems += block->id;
        attribute.usedItems += block->parent->id;
        attribute.stride = getBlockStride(*block);
        attribute.offset = blockOffset + getFieldRowOffset(field);
    }

    if (!validateAttribute(attribute)) {
        mMessages += MessageList::insert(field.id,
            MessageType::InvalidAttribute);
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
        case 4:
            break;
        default: 
            return false;
    }
    return true;
}

void VKStream::invalidateVertexOptions()
{
    mVertexOptions = { };
    mBuffers = { };
}

void VKStream::updateVertexOptions()
{
    if (!mBuffers.empty())
        return;

    using Key = std::tuple<VKBuffer*, int>;
    auto buffers = std::vector<Key>{ };
    auto location = 0;
    for (const auto &attribute : mAttributes) {
        const auto key = Key{ attribute.buffer, attribute.stride };
        auto it = std::find(begin(buffers), end(buffers), key);
        if (it == buffers.end()) {
            mVertexOptions.buffers.push_back(KDGpu::VertexBufferLayout{
                .binding = static_cast<uint32_t>(buffers.size()),
                .stride = static_cast<uint32_t>(attribute.stride),
                .inputRate = KDGpu::VertexRate::Vertex,
            });
            it = buffers.insert(buffers.end(), key);
            mBuffers.push_back(attribute.buffer);
        }
        const auto binding = std::distance(begin(buffers), it);
        mVertexOptions.attributes.push_back(KDGpu::VertexAttribute{
            .location = static_cast<uint32_t>(location++),
            .binding = static_cast<uint32_t>(binding),
            .format = toKDGpu(attribute.type, attribute.count),
            .offset = static_cast<KDGpu::DeviceSize>(attribute.offset),
        });
    }
}

const std::vector<VKBuffer*> &VKStream::getBuffers()
{
    updateVertexOptions();
    return mBuffers;
}

const KDGpu::VertexOptions &VKStream::getVertexOptions()
{
    updateVertexOptions();
    return mVertexOptions;
}
