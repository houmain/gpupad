#include "GLCall.h"
#include "GLTexture.h"
#include "GLBuffer.h"
#include "GLProgram.h"
#include "GLTarget.h"
#include "GLStream.h"
#include "Renderer.h"
#include "scripting/ScriptEngine.h"
#include <QOpenGLTimerQuery>
#include <QOpenGLVertexArrayObject>

GLCall::GLCall(const Call &call)
    : mCall(call)
{
}

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
    }

    auto &gl = GLContext::currentContext();
    if (gl.v4_2)
        gl.v4_2->glMemoryBarrier(GL_ALL_BARRIER_BITS);

    const auto error = glGetError();
    if (error != GL_NO_ERROR)
        messages += MessageList::insert(
            mCall.id, MessageType::CallFailed, getFirstGLError());
}

void GLCall::executeDraw(MessagePtrSet &messages, ScriptEngine &scriptEngine)
{
    const auto evaluate = [&](const auto &expression) {
        return static_cast<int>(scriptEngine.evaluateValue(
            expression, mCall.id, messages));
    };

    QOpenGLVertexArrayObject vao;
    QOpenGLVertexArrayObject::Binder vaoBinder(&vao);

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

    if (mVertexStream)
        mUsedItems += mVertexStream->usedItems();

    if (mTarget)
        mUsedItems += mTarget->usedItems();
}

void GLCall::executeCompute(MessagePtrSet &messages, ScriptEngine &scriptEngine)
{
    if (!mProgram) {
        messages += MessageList::insert(
            mCall.id, MessageType::ProgramNotAssigned);
        return;
    }

    const auto evaluate = [&](const auto &expression) {
        return static_cast<GLuint>(scriptEngine.evaluateValue(
            expression, mCall.id, messages));
    };
    auto &gl = GLContext::currentContext();
    auto guard = beginTimerQuery();
    if (auto gl43 = check(gl.v4_3, mCall.id, messages))
        gl43->glDispatchCompute(
            evaluate(mCall.workGroupsX),
            evaluate(mCall.workGroupsY),
            evaluate(mCall.workGroupsZ));
}

void GLCall::executeClearTexture(MessagePtrSet &messages)
{
    if (!mTexture) {
        messages += MessageList::insert(
            mCall.id, MessageType::TextureNotAssigned);
        return;
    }

    auto guard = beginTimerQuery();
    const auto color = std::array<double, 4>{
        mCall.clearColor.redF(),
        mCall.clearColor.greenF(),
        mCall.clearColor.blueF(),
        mCall.clearColor.alphaF()
    };
    if (!mTexture->clear(color, mCall.clearDepth, mCall.clearStencil))
        messages += MessageList::insert(
            mCall.id, MessageType::ClearingTextureFailed);

    mUsedItems += mTexture->usedItems();
}

void GLCall::executeCopyTexture(MessagePtrSet &messages)
{
    if (!mTexture || !mFromTexture) {
        messages += MessageList::insert(
            mCall.id, MessageType::TextureNotAssigned);
        return;
    }
    auto guard = beginTimerQuery();
    if (!mTexture->copy(*mFromTexture))
        messages += MessageList::insert(
            mCall.id, MessageType::CopyingTextureFailed);

    mUsedItems += mTexture->usedItems();
    mUsedItems += mFromTexture->usedItems();
}

void GLCall::executeClearBuffer(MessagePtrSet &messages)
{
    if (!mBuffer) {
        messages += MessageList::insert(
            mCall.id, MessageType::BufferNotAssigned);
        return;
    }
    auto guard = beginTimerQuery();
    mBuffer->clear();
    mUsedItems += mBuffer->usedItems();
}

void GLCall::executeCopyBuffer(MessagePtrSet &messages)
{
    if (!mBuffer || !mFromBuffer) {
        messages += MessageList::insert(
            mCall.id, MessageType::BufferNotAssigned);
        return;
    }
    auto guard = beginTimerQuery();
    mBuffer->copy(*mFromBuffer);
    mUsedItems += mBuffer->usedItems();
    mUsedItems += mFromBuffer->usedItems();
}
