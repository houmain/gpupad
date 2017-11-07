#include "GLCall.h"
#include "GLTexture.h"
#include "GLBuffer.h"
#include "GLProgram.h"
#include "GLTarget.h"
#include "GLStream.h"
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
            mIndicesOffset =
                static_cast<uintptr_t>(mCall.first * getStride(buffer));
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

void GLCall::setBuffer(GLBuffer *buffer)
{
    mBuffer = buffer;
}

void GLCall::setTexture(GLTexture *texture)
{
    mTexture = texture;
}

std::chrono::nanoseconds GLCall::duration() const
{
    if (!mTimerQuery)
        return std::chrono::nanoseconds(-1);
    return std::chrono::nanoseconds(mTimerQuery->waitForResult());
}

QOpenGLTimerQuery &GLCall::timerQuery()
{
    if (!mTimerQuery) {
        mTimerQuery = std::make_shared<QOpenGLTimerQuery>();
        mTimerQuery->create();
    }
    return *mTimerQuery;
}

void GLCall::execute()
{
    mMessages.clear();

    switch (mCall.callType) {
        case Call::CallType::Draw:
        case Call::CallType::DrawIndexed:
        case Call::CallType::DrawIndirect:
        case Call::CallType::DrawIndexedIndirect:
            return executeDraw();
        case Call::CallType::Compute:
            return executeCompute();
        case Call::CallType::ClearTexture:
            return executeClearTexture();
        case Call::CallType::ClearBuffer:
            return executeClearBuffer();
        case Call::CallType::GenerateMipmaps:
            return executeGenerateMipmaps();
    }

    auto &gl = GLContext::currentContext();
    if (gl.v4_2)
      gl.v4_2->glMemoryBarrier(GL_ALL_BARRIER_BITS);
}

void GLCall::executeDraw()
{
    if (mTarget)
        mTarget->bind();

    if (mVertexStream && mProgram)
        mVertexStream->bind(*mProgram);

    if (mIndexBuffer)
        mIndexBuffer->bindReadOnly(GL_ELEMENT_ARRAY_BUFFER);

    if (mProgram) {
        timerQuery().begin();
        auto &gl = GLContext::currentContext();

        if (mCall.primitiveType == Call::PrimitiveType::Patches && gl.v4_0)
            gl.v4_0->glPatchParameteri(GL_PATCH_VERTICES, mCall.patchVertices);

        if (mCall.callType == Call::CallType::Draw) {
            // DrawArrays(InstancedBaseInstance)
            if (!mCall.baseInstance) {
                gl.glDrawArraysInstanced(
                    mCall.primitiveType,
                    mCall.first,
                    mCall.count,
                    mCall.instanceCount);
            }
            else if (auto gl42 = check(gl.v4_2, mCall.id, mMessages)) {
                gl42->glDrawArraysInstancedBaseInstance(
                    mCall.primitiveType,
                    mCall.first,
                    mCall.count,
                    mCall.instanceCount,
                    static_cast<GLuint>(mCall.baseInstance));
            }
        }
        else if (mCall.callType == Call::CallType::DrawIndexed) {
            // DrawElements(InstancedBaseVertexBaseInstance)
            if (!mCall.baseInstance && !mCall.baseVertex) {
                gl.glDrawElementsInstanced(
                    mCall.primitiveType,
                    mCall.count,
                    mIndexType,
                    reinterpret_cast<void*>(mIndicesOffset),
                    mCall.instanceCount);
            }
            else if (auto gl42 = check(gl.v4_2, mCall.id, mMessages)) {
                gl42->glDrawElementsInstancedBaseVertexBaseInstance(
                    mCall.primitiveType,
                    mCall.count,
                    mIndexType,
                    reinterpret_cast<void*>(mIndicesOffset),
                    mCall.instanceCount,
                    mCall.baseVertex,
                    static_cast<GLuint>(mCall.baseInstance));
            }
        }
        else if (mCall.callType == Call::CallType::DrawIndirect) {
            // (Multi)DrawArraysIndirect
            if (mCall.drawCount == 1) {
                if (auto gl40 = check(gl.v4_0, mCall.id, mMessages))
                    gl40->glDrawArraysIndirect(mCall.primitiveType,
                        reinterpret_cast<void*>(mIndirectOffset));
            }
            else if (auto gl43 = check(gl.v4_3, mCall.id, mMessages)) {
                gl43->glMultiDrawArraysIndirect(mCall.primitiveType,
                    reinterpret_cast<void*>(mIndirectOffset), mCall.drawCount, mIndirectStride);
            }
        }
        else if (mCall.callType == Call::CallType::DrawIndexedIndirect) {
            // (Multi)DrawElementsIndirect
            if (mCall.drawCount == 1) {
                if (auto gl40 = check(gl.v4_0, mCall.id, mMessages))
                    gl40->glDrawElementsIndirect(mCall.primitiveType, mIndexType,
                        reinterpret_cast<void*>(mIndirectOffset));
            }
            else if (auto gl43 = check(gl.v4_3, mCall.id, mMessages)) {
                gl43->glMultiDrawElementsIndirect(mCall.primitiveType, mIndexType,
                    reinterpret_cast<void*>(mIndirectOffset), mCall.drawCount, mIndirectStride);
            }
        }
        timerQuery().end();
    }
    else {
        mMessages += MessageList::insert(
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

void GLCall::executeCompute()
{
    if (mProgram) {
        auto &gl = GLContext::currentContext();
        timerQuery().begin();
        if (auto gl43 = check(gl.v4_3, mCall.id, mMessages))
            gl43->glDispatchCompute(
                static_cast<GLuint>(mCall.workGroupsX),
                static_cast<GLuint>(mCall.workGroupsY),
                static_cast<GLuint>(mCall.workGroupsZ));
        timerQuery().end();
    }
    else {
        mMessages += MessageList::insert(
            mCall.id, MessageType::ProgramNotAssigned);
    }
}

void GLCall::executeClearTexture()
{
    if (mTexture) {
        mTexture->clear(mCall.clearColor,
            mCall.clearDepth, mCall.clearStencil);
        mUsedItems += mTexture->usedItems();
    }
    else {
        mMessages += MessageList::insert(
            mCall.id,  MessageType::TextureNotAssigned);
    }
}

void GLCall::executeClearBuffer()
{
    if (mBuffer) {
        mBuffer->clear();
        mUsedItems += mBuffer->usedItems();
    }
    else {
        mMessages += MessageList::insert(
            mCall.id, MessageType::BufferNotAssigned);
    }
}

void GLCall::executeGenerateMipmaps()
{
    if (mTexture) {
        mTexture->generateMipmaps();
        mUsedItems += mTexture->usedItems();
    }
    else {
        mMessages += MessageList::insert(
            mCall.id,  MessageType::TextureNotAssigned);
    }
}
