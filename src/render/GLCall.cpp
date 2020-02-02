#include "GLCall.h"
#include "GLTexture.h"
#include "GLBuffer.h"
#include "GLProgram.h"
#include "GLTarget.h"
#include "GLStream.h"
#include "scripting/ScriptEngine.h"
#include <QOpenGLTimerQuery>

GLCall::GLCall(const Call &call) : mCall(call) { }

void GLCall::setProgram(GLProgram *program)
{
    mProgram = program;
}

void GLCall::setTarget(GLTarget *target)
{
    mTarget = target;
}

void GLCall::setVextexStream(GLStream *stream)
{
    mVertexStream = stream;
}

void GLCall::setIndexBuffer(GLBuffer *indices, const Buffer &buffer)
{
    mUsedItems += buffer.id;
    if (!buffer.items.empty())
        if (const auto *column = castItem<Column>(buffer.items.first())) {
            mUsedItems += column->id;
            mIndexBuffer = indices;
            mIndexType = column->dataType;
            mIndexSize = getStride(buffer);
        }
}

void GLCall::setIndirectBuffer(GLBuffer *commands, const Buffer &buffer)
{
    mUsedItems += buffer.id;
    for (const auto *item : buffer.items)
        mUsedItems += item->id;
    mIndirectBuffer = commands;
    mIndirectStride = getStride(buffer);
    mIndirectOffset = 0;
}

void GLCall::setBuffers(GLBuffer *buffer, GLBuffer *fromBuffer)
{
    mBuffer = buffer;
    mFromBuffer = fromBuffer;
}

void GLCall::setTextures(GLTexture *texture, GLTexture *fromTexture)
{
    mTexture = texture;
    mFromTexture = fromTexture;
}

std::shared_ptr<void> GLCall::beginTimerQuery()
{
    mTimerQuery = std::make_shared<QOpenGLTimerQuery>();
    mTimerQuery->create();

    mTimerQuery->begin();
    return std::shared_ptr<void>(nullptr,
        [this](void*) { mTimerQuery->end(); });
}

void GLCall::execute(MessagePtrSet &messages, ScriptEngine &scriptEngine)
{
    switch (mCall.callType) {
        case Call::CallType::Draw:
        case Call::CallType::DrawIndexed:
        case Call::CallType::DrawIndirect:
        case Call::CallType::DrawIndexedIndirect:
            executeDraw(messages, scriptEngine);
            break;
        case Call::CallType::Compute:
            executeCompute(messages, scriptEngine);
            break;
        case Call::CallType::ClearTexture:
            executeClearTexture(messages);
            break;
        case Call::CallType::CopyTexture:
            executeCopyTexture(messages);
            break;
        case Call::CallType::ClearBuffer:
            executeClearBuffer(messages);
            break;
        case Call::CallType::CopyBuffer:
            executeCopyBuffer(messages);
            break;
        case Call::CallType::GenerateMipmaps:
            executeGenerateMipmaps(messages);
            break;
    }

    auto &gl = GLContext::currentContext();
    if (gl.v4_2)
      gl.v4_2->glMemoryBarrier(GL_ALL_BARRIER_BITS);
}

void GLCall::executeDraw(MessagePtrSet &messages, ScriptEngine &scriptEngine)
{
    const auto evaluate = [&](const auto &expression) {
        return static_cast<int>(scriptEngine.evaluateValue(
            expression, mCall.id, messages));
    };

    if (mTarget)
        mTarget->bind();

    if (mVertexStream && mProgram)
        mVertexStream->bind(*mProgram);

    if (mIndexBuffer)
        mIndexBuffer->bindReadOnly(GL_ELEMENT_ARRAY_BUFFER);

    if (mProgram) {
        auto &gl = GLContext::currentContext();

        if (mCall.primitiveType == Call::PrimitiveType::Patches && gl.v4_0)
            gl.v4_0->glPatchParameteri(GL_PATCH_VERTICES, 
              evaluate(mCall.patchVertices));

        const auto first = evaluate(mCall.first);
        const auto count = evaluate(mCall.count);
        const auto instanceCount = evaluate(mCall.instanceCount);
        const auto baseVertex = evaluate(mCall.baseVertex);
        const auto baseInstance = evaluate(mCall.baseInstance);
        const auto drawCount = evaluate(mCall.drawCount);

        auto guard = beginTimerQuery();
        if (mCall.callType == Call::CallType::Draw) {
            // DrawArrays(InstancedBaseInstance)
            if (!baseInstance) {
                gl.glDrawArraysInstanced(
                    mCall.primitiveType,
                    first,
                    count,
                    instanceCount);
            }
            else if (auto gl42 = check(gl.v4_2, mCall.id, messages)) {
                gl42->glDrawArraysInstancedBaseInstance(
                    mCall.primitiveType,
                    first,
                    count,
                    instanceCount,
                    static_cast<GLuint>(baseInstance));
            }
        }
        else if (mCall.callType == Call::CallType::DrawIndexed) {
            // DrawElements(InstancedBaseVertexBaseInstance)
            const auto offset = static_cast<intptr_t>(first * mIndexSize);
            if (!baseVertex && !baseInstance) {
                gl.glDrawElementsInstanced(
                    mCall.primitiveType,
                    count,
                    mIndexType,
                    reinterpret_cast<void*>(offset),
                    instanceCount);
            }
            else if (auto gl42 = check(gl.v4_2, mCall.id, messages)) {
                gl42->glDrawElementsInstancedBaseVertexBaseInstance(
                    mCall.primitiveType,
                    count,
                    mIndexType,
                    reinterpret_cast<void*>(offset),
                    instanceCount,
                    baseVertex,
                    static_cast<GLuint>(baseInstance));
            }
        }
        else if (mCall.callType == Call::CallType::DrawIndirect) {
            // (Multi)DrawArraysIndirect
            if (drawCount == 1) {
                if (auto gl40 = check(gl.v4_0, mCall.id, messages))
                    gl40->glDrawArraysIndirect(mCall.primitiveType,
                        reinterpret_cast<void*>(mIndirectOffset));
            }
            else if (auto gl43 = check(gl.v4_3, mCall.id, messages)) {
                gl43->glMultiDrawArraysIndirect(mCall.primitiveType,
                    reinterpret_cast<void*>(mIndirectOffset), 
                    drawCount, mIndirectStride);
            }
        }
        else if (mCall.callType == Call::CallType::DrawIndexedIndirect) {
            // (Multi)DrawElementsIndirect
            if (drawCount == 1) {
                if (auto gl40 = check(gl.v4_0, mCall.id, messages))
                    gl40->glDrawElementsIndirect(mCall.primitiveType, mIndexType,
                        reinterpret_cast<void*>(mIndirectOffset));
            }
            else if (auto gl43 = check(gl.v4_3, mCall.id, messages)) {
                gl43->glMultiDrawElementsIndirect(mCall.primitiveType, mIndexType,
                    reinterpret_cast<void*>(mIndirectOffset), 
                    drawCount, mIndirectStride);
            }
        }
    }
    else {
        messages += MessageList::insert(
            mCall.id,  MessageType::ProgramNotAssigned);
    }

    if (mIndexBuffer)
        mIndexBuffer->unbind(GL_ELEMENT_ARRAY_BUFFER);

    if (mVertexStream) {
        mVertexStream->unbind();
        mUsedItems += mVertexStream->usedItems();
    }

    if (mTarget) {
        mTarget->unbind();
        mUsedItems += mTarget->usedItems();
    }
}

void GLCall::executeCompute(MessagePtrSet &messages, ScriptEngine &scriptEngine)
{
    const auto evaluate = [&](const auto &expression) {
        return static_cast<GLuint>(scriptEngine.evaluateValue(
            expression, mCall.id, messages));
    };
    if (mProgram) {
        auto &gl = GLContext::currentContext();
        auto guard = beginTimerQuery();
        if (auto gl43 = check(gl.v4_3, mCall.id, messages))
            gl43->glDispatchCompute(
                evaluate(mCall.workGroupsX),
                evaluate(mCall.workGroupsY),
                evaluate(mCall.workGroupsZ));
    }
    else {
        messages += MessageList::insert(
            mCall.id, MessageType::ProgramNotAssigned);
    }
}

void GLCall::executeClearTexture(MessagePtrSet &messages)
{
    if (mTexture) {
        auto guard = beginTimerQuery();
        mTexture->clear(mCall.clearColor,
            mCall.clearDepth, mCall.clearStencil);
        mUsedItems += mTexture->usedItems();
    }
    else {
        messages += MessageList::insert(
            mCall.id,  MessageType::TextureNotAssigned);
    }
}

void GLCall::executeCopyTexture(MessagePtrSet &messages)
{
    if (mTexture && mFromTexture) {
        auto guard = beginTimerQuery();
        mTexture->copy(*mFromTexture);
        mUsedItems += mTexture->usedItems();
        mUsedItems += mFromTexture->usedItems();
    }
    else {
        messages += MessageList::insert(
            mCall.id,  MessageType::TextureNotAssigned);
    }
}

void GLCall::executeClearBuffer(MessagePtrSet &messages)
{
    if (mBuffer) {
        auto guard = beginTimerQuery();
        mBuffer->clear();
        mUsedItems += mBuffer->usedItems();
    }
    else {
        messages += MessageList::insert(
            mCall.id, MessageType::BufferNotAssigned);
    }
}

void GLCall::executeCopyBuffer(MessagePtrSet &messages)
{
    if (mBuffer && mFromBuffer) {
        auto guard = beginTimerQuery();
        mBuffer->copy(*mFromBuffer);
        mUsedItems += mBuffer->usedItems();
        mUsedItems += mFromBuffer->usedItems();
    }
    else {
        messages += MessageList::insert(
            mCall.id,  MessageType::BufferNotAssigned);
    }
}
void GLCall::executeGenerateMipmaps(MessagePtrSet &messages)
{
    if (mTexture) {
        auto guard = beginTimerQuery();
        mTexture->generateMipmaps();
        mUsedItems += mTexture->usedItems();
    }
    else {
        messages += MessageList::insert(
            mCall.id,  MessageType::TextureNotAssigned);
    }
}
