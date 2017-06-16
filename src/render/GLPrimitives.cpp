#include "GLPrimitives.h"

void GLPrimitives::initialize(PrepareContext &context,
    const Primitives &primitives)
{
    context.usedItems += primitives.id;

    mType = primitives.type;
    mFirstVertex = primitives.firstVertex;
    mVertexCount = primitives.vertexCount;
    mInstanceCount = primitives.instanceCount;
    mPatchVertices = primitives.patchVertices;
    mPrimitiveRestartIndex = primitives.primitiveRestartIndex;

    for (const auto& item : primitives.items)
        if (auto attribute = castItem<Attribute>(item)) {
            context.usedItems += attribute->id;

            auto buffer = context.session.findItem<Buffer>(attribute->bufferId);
            auto column = context.session.findItem<Column>(attribute->columnId);
            if (buffer && column) {
                context.usedItems += column->id;
                context.usedItems += buffer->id;

                mAttributes.push_back(GLAttribute{
                    addOnce(mBuffers, *buffer, context),
                    attribute->name,
                    column->count,
                    column->dataType,
                    attribute->normalize,
                    buffer->stride(),
                    buffer->columnOffset(column),
                    attribute->divisor
                });
            }
        }

    if (primitives.indexBufferId)
        if (auto buffer = context.session.findItem<Buffer>(primitives.indexBufferId)) {
            context.usedItems += buffer->id;

            mIndexBufferIndex = addOnce(mBuffers, *buffer, context);

            if (!buffer->items.empty())
                if (auto column = castItem<Column>(buffer->items.first())) {
                    context.usedItems.insert(column->id);
                    mIndexType = column->dataType;
                    mIndicesOffset = buffer->columnOffset(column) +
                        mFirstVertex * column->size();
                }
        }
}

void GLPrimitives::cache(RenderContext &context, GLPrimitives &&update)
{
    Q_UNUSED(context);
    QOpenGLVertexArrayObject::Binder vaoBinder(&mVertexArrayObject);

    mType = update.mType;
    mFirstVertex = update.mFirstVertex;
    mVertexCount = update.mVertexCount;
    mInstanceCount = update.mInstanceCount;
    mPatchVertices = update.mPatchVertices;
    mPrimitiveRestartIndex = update.mPrimitiveRestartIndex;
    mIndexBufferIndex = update.mIndexBufferIndex;
    mIndexType = update.mIndexType;
    mIndicesOffset = update.mIndicesOffset;

    updateList(mBuffers, std::move(update.mBuffers));

    mAttributes = std::move(update.mAttributes);
}

void GLPrimitives::draw(RenderContext &context, const GLProgram &program)
{
    QOpenGLVertexArrayObject::Binder vaoBinder(&mVertexArrayObject);

    auto enabledVertexAttributes = std::vector<int>();
    for (const auto& attribute : mAttributes) {
        auto location = program.getAttributeLocation(context, attribute.name);
        if (location < 0)
            continue;

        auto& buffer = mBuffers.at(attribute.bufferIndex);
        buffer.bindReadOnly(context, GL_ARRAY_BUFFER);

        context.glVertexAttribPointer(
            location,
            attribute.count,
            attribute.type,
            attribute.normalize,
            attribute.stride,
            reinterpret_cast<void*>(static_cast<intptr_t>(attribute.offset)));

        context.glVertexAttribDivisor(location, attribute.divisor);

        context.glEnableVertexAttribArray(location);
        enabledVertexAttributes.push_back(location);

        buffer.unbind(context, GL_ARRAY_BUFFER);
    }

    if (mType == Primitives::Patches) {
        if (context.gl40)
            context.gl40->glPatchParameteri(GL_PATCH_VERTICES, mPatchVertices);
    }

    if (mIndexBufferIndex >= 0) {
        auto &ib = mBuffers.at(mIndexBufferIndex);
        ib.bindReadOnly(context, GL_ELEMENT_ARRAY_BUFFER);
        context.glDrawElementsInstanced(mType, mVertexCount, mIndexType,
            reinterpret_cast<void*>(static_cast<intptr_t>(mIndicesOffset)), mInstanceCount);

        ib.unbind(context, GL_ELEMENT_ARRAY_BUFFER);
    }
    else {
        context.glDrawArraysInstanced(mType, mFirstVertex,
            mVertexCount, mInstanceCount);
    }

    for (auto location : enabledVertexAttributes)
        context.glDisableVertexAttribArray(location);
}

