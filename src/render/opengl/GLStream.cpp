#include "GLStream.h"

GLStream::GLStream(const Stream &stream) : mItemId(stream.id)
{
    auto attributeIndex = 0;
    for (const auto &item : stream.items) {
        if (auto attribute = castItem<Attribute>(item))
            mAttributes[attributeIndex] = GLAttribute{
                { item->id },
                attribute->name,
                attribute->normalize,
                attribute->divisor,
            };
        ++attributeIndex;
    }
}

void GLStream::setAttribute(int attributeIndex, const Field &field,
    GLBuffer *buffer, ScriptEngine &scriptEngine)
{
    const auto &block = *castItem<Block>(field.parent);
    const auto blockOffset =
        scriptEngine.evaluateValue(block.offset, block.id);
    auto &attribute = mAttributes[attributeIndex];
    attribute.usedItems += field.id;
    attribute.usedItems += block.id;
    attribute.usedItems += block.parent->id;
    attribute.buffer = buffer;
    attribute.type = field.dataType;
    attribute.count = field.count;
    attribute.stride = getBlockStride(block);
    attribute.offset = blockOffset + getFieldRowOffset(field);

    const auto rowCount =
        scriptEngine.evaluateValue(block.rowCount, block.id);
    if (mMaxElementCount < 0 || rowCount < mMaxElementCount)
        mMaxElementCount = rowCount;

    if (!validateAttribute(attribute)) {
        attribute.buffer = nullptr;
        mMessages +=
            MessageList::insert(field.id, MessageType::InvalidAttribute);
    }
}

bool GLStream::validateAttribute(const GLAttribute &attribute) const
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

const GLAttribute *GLStream::findAttribute(const QString &name) const
{
    for (auto &attribute : mAttributes)
        if (attribute.name == name)
            return &attribute;
    return nullptr;
}
