
#include "D3DCall.h"
#include "D3DBuffer.h"
#include "D3DProgram.h"
#include "D3DStream.h"
#include "D3DTarget.h"
#include "D3DTexture.h"
#include <QScopeGuard>
#include <cmath>

D3DCall::D3DCall(const Call &call, const Session &session)
    : mCall(call)
    , mKind(getKind(mCall))
    , mSession(session)
{
    mUsedItems += session.id;
}

D3DCall::~D3DCall() = default;

void D3DCall::setProgram(D3DProgram *program)
{
    mProgram = program;
}

void D3DCall::setTarget(D3DTarget *target)
{
    mTarget = target;
}

void D3DCall::setVextexStream(D3DStream *stream)
{
    mVertexStream = stream;
}

void D3DCall::setIndexBuffer(D3DBuffer *indices, const Block &block)
{
    if (!mKind.indexed)
        return;

    mUsedItems += block.id;
    mUsedItems += block.parent->id;
    auto indexSize = 0;
    for (auto item : block.items)
        if (auto field = castItem<Field>(item)) {
            indexSize += getFieldSize(*field);
            mUsedItems += field->id;
        }

    mIndexSize = indexSize;
    mIndexBuffer = indices;
    mIndicesOffset = block.offset;
    mIndicesRowCount = block.rowCount;
    mIndicesPerRow = getBlockStride(block) / indexSize;
}

void D3DCall::setIndirectBuffer(D3DBuffer *commands, const Block &block)
{
    if (!mKind.indirect)
        return;

    mUsedItems += block.id;
    mUsedItems += block.parent->id;
    for (auto field : block.items)
        mUsedItems += field->id;

    mIndirectStride = getBlockStride(block);
    const auto expectedStride =
        static_cast<int>((mKind.compute ? 3 : 4) * sizeof(uint32_t));
    if (mIndirectStride != expectedStride) {
        mMessages += MessageList::insert(block.id,
            MessageType::InvalidIndirectStride,
            QStringLiteral("%1/%2 bytes")
                .arg(mIndirectStride)
                .arg(expectedStride));
        return;
    }

    mIndirectBuffer = commands;
    mIndirectOffset = block.offset;
}

void D3DCall::setBuffers(D3DBuffer *buffer, D3DBuffer *fromBuffer)
{
    mBuffer = buffer;
    mFromBuffer = fromBuffer;
}

void D3DCall::setTextures(D3DTexture *texture, D3DTexture *fromTexture)
{
    mTexture = texture;
    mFromTexture = fromTexture;
}

void D3DCall::setAccelerationStructure(D3DAccelerationStructure *accelStruct)
{
    mAccelerationStructure = accelStruct;
}

bool D3DCall::validateShaderTypes()
{
    if (!mProgram)
        return false;
    for (const auto &shader : mProgram->shaders())
        if (!callTypeSupportsShaderType(mCall.callType, shader.type())) {
            mMessages += MessageList::insert(mCall.id,
                MessageType::InvalidShaderTypeForCall);
            return false;
        }
    return true;
}

void D3DCall::execute(D3DContext &context, Bindings &&bindings,
    MessagePtrSet &messages, ScriptEngine &scriptEngine)
{
    if (mKind.trace) {
        mMessages += MessageList::insert(mCall.id, MessageType::NotImplemented,
            "Ray Tracing");
        return;
    }

    if (mKind.mesh) {
        mMessages += MessageList::insert(mCall.id, MessageType::NotImplemented,
            "Mesh Shaders");
        return;
    }

    if (mKind.draw || mKind.compute || mKind.trace) {
        if (!mProgram) {
            messages +=
                MessageList::insert(mCall.id, MessageType::ProgramNotAssigned);
            return;
        }
        mUsedItems += mProgram->usedItems();
    }

    if (mProgram && !mPipeline) {
        if (!validateShaderTypes())
            return;

        if (!mProgram->link(context))
            return;

        if (mVertexStream)
            mUsedItems += mVertexStream->usedItems();

        mPipeline = std::make_unique<D3DPipeline>(mCall.id, mProgram);
    }

    if (mKind.draw) {
        if (!mTarget) {
            messages +=
                MessageList::insert(mCall.id, MessageType::TargetNotAssigned);
            return;
        }
        mUsedItems += mTarget->usedItems();
    }

    if (mKind.indexed && !mIndexBuffer) {
        messages +=
            MessageList::insert(mCall.id, MessageType::IndexBufferNotAssigned);
        return;
    }

    if (mKind.trace && !mAccelerationStructure) {
        messages += MessageList::insert(mCall.id,
            MessageType::AccelerationStructureNotAssigned);
        return;
    }

    if (mKind.indirect && !mIndirectBuffer) {
        messages += MessageList::insert(mCall.id,
            MessageType::IndirectBufferNotAssigned);
        return;
    }

    if (mPipeline)
        mPipeline->setBindings(std::move(bindings));

    switch (mCall.callType) {
    case Call::CallType::Draw:
    case Call::CallType::DrawIndexed:
    case Call::CallType::DrawIndirect:
    case Call::CallType::DrawIndexedIndirect:
    case Call::CallType::DrawMeshTasks:
    case Call::CallType::DrawMeshTasksIndirect:
        executeDraw(context, messages, scriptEngine);
        break;
    case Call::CallType::Compute:
    case Call::CallType::ComputeIndirect:
        executeCompute(context, messages, scriptEngine);
        break;
    case Call::CallType::TraceRays:
        executeTraceRays(context, messages, scriptEngine);
        break;
    case Call::CallType::ClearTexture:
        executeClearTexture(context, messages);
        break;
    case Call::CallType::CopyTexture:
        executeCopyTexture(context, messages);
        break;
    case Call::CallType::ClearBuffer:
        executeClearBuffer(context, messages);
        break;
    case Call::CallType::CopyBuffer:
        executeCopyBuffer(context, messages);
        break;
    case Call::CallType::SwapTextures: executeSwapTextures(messages); break;
    case Call::CallType::SwapBuffers:  executeSwapBuffers(messages); break;
    }
}

int D3DCall::getMaxElementCount(ScriptEngine &scriptEngine)
{
    if (mKind.indexed)
        return scriptEngine.evaluateInt(mIndicesRowCount, mCall.id)
            * mIndicesPerRow;
    if (mVertexStream)
        return mVertexStream->maxElementCount();
    return -1;
}

void D3DCall::executeDraw(D3DContext &context, MessagePtrSet &messages,
    ScriptEngine &scriptEngine)
{
    const auto first = scriptEngine.evaluateUInt(mCall.first, mCall.id);
    const auto maxElementCount = getMaxElementCount(scriptEngine);
    const auto count = (!mCall.count.isEmpty()
            ? scriptEngine.evaluateUInt(mCall.count, mCall.id)
            : std::max(maxElementCount - static_cast<int>(first), 0));
    const auto instanceCount =
        scriptEngine.evaluateUInt(mCall.instanceCount, mCall.id);
    const auto baseVertex =
        scriptEngine.evaluateInt(mCall.baseVertex, mCall.id);
    const auto firstInstance =
        scriptEngine.evaluateUInt(mCall.baseInstance, mCall.id);
    const auto drawCount = scriptEngine.evaluateUInt(mCall.drawCount, mCall.id);
    const auto indirectOffset = (mKind.indirect
            ? scriptEngine.evaluateUInt(mIndirectOffset, mCall.id)
            : 0);

    if (!count)
        return;

    if (maxElementCount >= 0
        && first + count > static_cast<uint32_t>(maxElementCount)) {
        mMessages += MessageList::insert(mCall.id, MessageType::CountExceeded,
            first ? QStringLiteral("%1 + %2 > %3")
                        .arg(first)
                        .arg(count)
                        .arg(maxElementCount)
                  : QStringLiteral("%1 > %2").arg(count).arg(maxElementCount));
        return;
    }

    if (!mPipeline
        || !mPipeline->createGraphics(context, mCall.primitiveType, mTarget,
            mVertexStream)
        || !mPipeline->bindGraphics(context, scriptEngine))
        return;

    //if (mIndirectBuffer)
    //    mIndirectBuffer->prepareIndirectBuffer(context);

    //if (mIndexBuffer)
    //    mIndexBuffer->prepareIndexBuffer(context);

    //if (mIndexBuffer) {
    //    const auto indicesOffset =
    //        scriptEngine.evaluateUInt(mIndicesOffset, mCall.id);
    //    renderPass.setIndexBuffer(mIndexBuffer->buffer(), indicesOffset,
    //        mIndexType);
    //}

    context.graphicsCommandList->IASetPrimitiveTopology(
        toD3DPrimitiveTopology(mCall.primitiveType,
            scriptEngine.evaluateUInt(mCall.patchVertices, mCall.id)));

    if (mTarget)
        mTarget->bind(context);

    if (mCall.callType == Call::CallType::Draw) {
        context.graphicsCommandList->DrawInstanced(count, instanceCount, first,
            firstInstance);
    } else if (mCall.callType == Call::CallType::DrawIndexed) {
        context.graphicsCommandList->DrawIndexedInstanced(count, instanceCount,
            first, baseVertex, firstInstance);
    } else {
        mMessages += MessageList::insert(mCall.id, MessageType::NotImplemented,
            "Call Type");
    }
    mUsedItems += mPipeline->usedItems();
}

void D3DCall::executeCompute(D3DContext &context, MessagePtrSet &messages,
    ScriptEngine &scriptEngine)
{
    if (!mPipeline || !mPipeline->createCompute(context)
        || !mPipeline->bindCompute(context, scriptEngine))
        return;

    if (mCall.callType == Call::CallType::Compute) {
        context.graphicsCommandList->Dispatch(
            scriptEngine.evaluateUInt(mCall.workGroupsX, mCall.id),
            scriptEngine.evaluateUInt(mCall.workGroupsY, mCall.id),
            scriptEngine.evaluateUInt(mCall.workGroupsZ, mCall.id));
    }
    mUsedItems += mPipeline->usedItems();
}

void D3DCall::executeTraceRays(D3DContext &context, MessagePtrSet &messages,
    ScriptEngine &scriptEngine)
{
}

void D3DCall::executeClearTexture(D3DContext &context, MessagePtrSet &messages)
{
    if (!mTexture) {
        messages +=
            MessageList::insert(mCall.id, MessageType::TextureNotAssigned);
        return;
    }

    auto color = std::array<double, 4>{ mCall.clearColor.redF(),
        mCall.clearColor.greenF(), mCall.clearColor.blueF(),
        mCall.clearColor.alphaF() };

    const auto sampleType = getTextureSampleType(mTexture->format());
    if (sampleType == TextureSampleType::Float) {
        const auto srgbToLinear = [](double s) {
            if (s > 0.0031308)
                return 1.055 * (std::pow(s, (1.0 / 2.4))) - 0.055;
            return 12.92 * s;
        };
        color[0] = srgbToLinear(color[0]);
        color[1] = srgbToLinear(color[1]);
        color[2] = srgbToLinear(color[2]);
    }

    if (!mTexture->clear(context, color, mCall.clearDepth, mCall.clearStencil))
        messages +=
            MessageList::insert(mCall.id, MessageType::ClearingTextureFailed);

    mUsedItems += mTexture->usedItems();
}

void D3DCall::executeCopyTexture(D3DContext &context, MessagePtrSet &messages)
{
    if (!mTexture || !mFromTexture) {
        messages +=
            MessageList::insert(mCall.id, MessageType::TextureNotAssigned);
        return;
    }
    if (!mTexture->copy(context, *mFromTexture))
        messages +=
            MessageList::insert(mCall.id, MessageType::CopyingTextureFailed);

    mUsedItems += mTexture->usedItems();
    mUsedItems += mFromTexture->usedItems();
}

void D3DCall::executeClearBuffer(D3DContext &context, MessagePtrSet &messages)
{
    if (!mBuffer) {
        messages +=
            MessageList::insert(mCall.id, MessageType::BufferNotAssigned);
        return;
    }
    mBuffer->clear(context);
    mUsedItems += mBuffer->usedItems();
}

void D3DCall::executeCopyBuffer(D3DContext &context, MessagePtrSet &messages)
{
    if (!mBuffer || !mFromBuffer) {
        messages +=
            MessageList::insert(mCall.id, MessageType::BufferNotAssigned);
        return;
    }
    mBuffer->copy(context, *mFromBuffer);
    mUsedItems += mBuffer->usedItems();
    mUsedItems += mFromBuffer->usedItems();
}

void D3DCall::executeSwapTextures(MessagePtrSet &messages)
{
    if (!mTexture || !mFromTexture) {
        messages +=
            MessageList::insert(mCall.id, MessageType::TextureNotAssigned);
        return;
    }
    if (!mTexture->swap(*mFromTexture))
        messages +=
            MessageList::insert(mCall.id, MessageType::SwappingTexturesFailed);
}

void D3DCall::executeSwapBuffers(MessagePtrSet &messages)
{
    if (!mBuffer || !mFromBuffer) {
        messages +=
            MessageList::insert(mCall.id, MessageType::BufferNotAssigned);
        return;
    }
    if (!mBuffer->swap(*mFromBuffer))
        messages +=
            MessageList::insert(mCall.id, MessageType::SwappingBuffersFailed);
}
