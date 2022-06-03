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

void GLStream::setAttribute(int attributeIndex, const Field &field, 
    GLBuffer *buffer, ScriptEngine& scriptEngine)
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
        attribute.buffer = nullptr;
        mMessages += MessageList::insert(field.id,
            MessageType::InvalidAttribute);
    }
}

bool GLStream::validateAttribute(const GLAttribute &attribute) const
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

bool GLStream::bind(const GLProgram &program)
{
    auto succeeded = true;

    auto &gl = GLContext::currentContext();
    for (const GLAttribute &attribute : qAsConst(mAttributes)) {
        const auto attribLocation = program.getAttributeLocation(attribute.name);
        if (attribLocation < 0)
            continue;
        mUsedItems += attribute.usedItems;

        if (!attribute.buffer) {
            succeeded = false;
            continue;
        }

        const auto location = static_cast<GLuint>(attribLocation);
        attribute.buffer->bindReadOnly(GL_ARRAY_BUFFER);

        switch(attribute.type) {
            case GL_BYTE: case GL_UNSIGNED_BYTE: 
            case GL_SHORT: case GL_UNSIGNED_SHORT: 
            case GL_INT: case GL_UNSIGNED_INT:
                // normalized integer types fall through to glVertexAttribPointer
                if (!attribute.normalize) {
                    gl.glVertexAttribIPointer(
                        location,
                        attribute.count,
                        attribute.type,
                        attribute.stride,
                        reinterpret_cast<void*>(static_cast<intptr_t>(attribute.offset)));
                    break;
                }
                [[fallthrough]];

            default:
                gl.glVertexAttribPointer(
                    location,
                    attribute.count,
                    attribute.type,
                    attribute.normalize,
                    attribute.stride,
                    reinterpret_cast<void*>(static_cast<intptr_t>(attribute.offset)));
                break;

            case GL_DOUBLE:
                if (gl.v4_2)
                    gl.v4_2->glVertexAttribLPointer(
                        location,
                        attribute.count,
                        attribute.type,
                        attribute.stride,
                        reinterpret_cast<void*>(static_cast<intptr_t>(attribute.offset)));
                break;
        }

        gl.glVertexAttribDivisor(location,
            static_cast<GLuint>(attribute.divisor));

        gl.glEnableVertexAttribArray(location);
        attribute.buffer->unbind(GL_ARRAY_BUFFER);
    }
    return succeeded;
}

void GLStream::unbind(const GLProgram &program)
{
    auto &gl = GLContext::currentContext();
    for (const GLAttribute &attribute : qAsConst(mAttributes))
        if (const auto location = program.getAttributeLocation(attribute.name); location >= 0)
            gl.glDisableVertexAttribArray(static_cast<GLuint>(location));
}
