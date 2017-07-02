#include "GLPrimitives.h"

GLPrimitives::GLPrimitives(const Primitives &primitives)
{
    mUsedItems += primitives.id;
    mType = primitives.type;
    mFirstVertex = primitives.firstVertex;
    mVertexCount = primitives.vertexCount;
    mInstanceCount = primitives.instanceCount;
    mPatchVertices = primitives.patchVertices;
    mPrimitiveRestartIndex = primitives.primitiveRestartIndex;

    for (const auto& item : primitives.items)
        if (auto attribute = castItem<Attribute>(item)) {
            mUsedItems += attribute->id;
            mAttributes.push_back(GLAttribute{
                attribute->id,
                attribute->name,
                attribute->normalize,
                attribute->divisor,
                nullptr, Column::DataType(), 0, 0, 0 });
        }
}

void GLPrimitives::setAttribute(int attributeIndex,
    const Column &column, GLBuffer *buffer)
{
    auto& attribute = mAttributes.at(attributeIndex);
    mUsedItems += column.id;
    attribute.buffer = buffer;
    attribute.type = column.dataType;
    attribute.count = column.count;
    if (auto b = castItem<Buffer>(column.parent)) {
        mUsedItems += b->id;
        attribute.stride = b->stride();
        attribute.offset = b->columnOffset(&column);
    }
}

void GLPrimitives::setIndices(const Column &column, GLBuffer *indices)
{
    mUsedItems += column.id;
    mIndexBuffer = indices;
    mIndexType = column.dataType;
    if (auto buffer = castItem<Buffer>(column.parent)) {
        mUsedItems += buffer->id;
        mIndicesOffset = buffer->columnOffset(&column) +
            mFirstVertex * column.size();
    }
}

void GLPrimitives::draw(const GLProgram &program)
{
    QOpenGLVertexArrayObject::Binder vaoBinder(&mVertexArrayObject);
    auto &gl = GLContext::currentContext();

    auto enabledVertexAttributes = std::vector<int>();
    for (const auto& attribute : mAttributes) {
        auto location = program.getAttributeLocation(attribute.name);
        if (location < 0)
            continue;

        mUsedItems += attribute.id;

        attribute.buffer->bindReadOnly(GL_ARRAY_BUFFER);

        gl.glVertexAttribPointer(
            location,
            attribute.count,
            attribute.type,
            attribute.normalize,
            attribute.stride,
            reinterpret_cast<void*>(static_cast<intptr_t>(attribute.offset)));

        gl.glVertexAttribDivisor(location, attribute.divisor);

        gl.glEnableVertexAttribArray(location);
        enabledVertexAttributes.push_back(location);

        attribute.buffer->unbind(GL_ARRAY_BUFFER);
    }

    if (mType == Primitives::Patches) {
        if (gl.v4_0)
            gl.v4_0->glPatchParameteri(GL_PATCH_VERTICES, mPatchVertices);
    }

    if (mIndexBuffer) {
        mIndexBuffer->bindReadOnly(GL_ELEMENT_ARRAY_BUFFER);
        gl.glDrawElementsInstanced(mType, mVertexCount, mIndexType,
            reinterpret_cast<void*>(static_cast<intptr_t>(mIndicesOffset)), mInstanceCount);

        mIndexBuffer->unbind(GL_ELEMENT_ARRAY_BUFFER);
    }
    else {
        gl.glDrawArraysInstanced(mType, mFirstVertex,
            mVertexCount, mInstanceCount);
    }

    for (auto location : enabledVertexAttributes)
        gl.glDisableVertexAttribArray(location);
}

