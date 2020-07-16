#include "GLStream.h"

GLStream::GLStream(const Stream &stream)
{
    mUsedItems += stream.id;

    auto attributeIndex = 0;
    for (const auto &item : stream.items) {
        if (auto attribute = castItem<Attribute>(item))
            mAttributes[attributeIndex] = GLAttribute{
                { item->id },
                attribute->name,
                attribute->normalize,
                attribute->divisor,
                nullptr, { }, 0, 0, 0 };
        ++attributeIndex;
    }
}

void GLStream::setAttribute(int attributeIndex,
    const Column &column, GLBuffer *buffer)
{
    auto &attribute = mAttributes[attributeIndex];
    attribute.usedItems += column.id;
    attribute.buffer = buffer;
    attribute.type = column.dataType;
    attribute.count = column.count;
    if (auto buf = castItem<Buffer>(column.parent)) {
        attribute.usedItems += buf->id;
        attribute.stride = getStride(*buf);
        attribute.offset = getColumnOffset(column);
    }
}

void GLStream::bind(const GLProgram &program)
{
    auto &gl = GLContext::currentContext();
    for (const GLAttribute &attribute : qAsConst(mAttributes)) {
        auto attribLocation = program.getAttributeLocation(attribute.name);
        if (attribLocation < 0)
            continue;
        auto location = static_cast<GLuint>(attribLocation);

        mUsedItems += attribute.usedItems;

        if (attribute.buffer) {
            attribute.buffer->bindReadOnly(GL_ARRAY_BUFFER);

            gl.glVertexAttribPointer(
                location,
                attribute.count,
                attribute.type,
                attribute.normalize,
                attribute.stride,
                reinterpret_cast<void*>(static_cast<intptr_t>(attribute.offset)));

            gl.glVertexAttribDivisor(location,
                static_cast<GLuint>(attribute.divisor));

            gl.glEnableVertexAttribArray(location);
            attribute.buffer->unbind(GL_ARRAY_BUFFER);
        }
    }
}
