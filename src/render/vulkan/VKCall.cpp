
#include "VKCall.h"
#include "VKTexture.h"
#include "VKBuffer.h"
#include "VKProgram.h"
#include "VKTarget.h"
#include "VKStream.h"
#include <cmath>

VKCall::VKCall(const Call &call)
    : mCall(call)
{
}

VKCall::~VKCall() = default;

void VKCall::setProgram(VKProgram *program)
{
    mProgram = program;
}

void VKCall::setTarget(VKTarget *target)
{
    mTarget = target;
}

void VKCall::setVextexStream(VKStream *stream)
{
    mVertexStream = stream;
}

void VKCall::setIndexBuffer(VKBuffer *indices, const Block &block)
{
    mUsedItems += block.id;
    mUsedItems += block.parent->id;
    for (auto field : block.items)
        mUsedItems += field->id;

    mIndexBuffer = indices;
    mIndexType = (getBlockStride(block) == 2 ? 
        KDGpu::IndexType::Uint16 : KDGpu::IndexType::Uint32);
    mIndicesOffset = block.offset;
}

void VKCall::setIndirectBuffer(VKBuffer *commands, const Block &block)
{
    mUsedItems += block.id;
    mUsedItems += block.parent->id;
    for (auto field : block.items)
        mUsedItems += field->id;

    mIndirectBuffer = commands;
    mIndirectStride = getBlockStride(block);
    mIndirectOffset = block.offset;
}

void VKCall::setBuffers(VKBuffer *buffer, VKBuffer *fromBuffer)
{
    mBuffer = buffer;
    mFromBuffer = fromBuffer;
}

void VKCall::setTextures(VKTexture *texture, VKTexture *fromTexture)
{
    mTexture = texture;
    mFromTexture = fromTexture;
}

VKPipeline *VKCall::createPipeline(VKContext &context)
{
    if (!mPipeline && mProgram) {
        mPipeline = std::make_unique<VKPipeline>(
            mCall.id, mProgram, mTarget, mVertexStream);

        mProgram->link(context.device);
        mUsedItems += mProgram->usedItems();

        if (mTarget)
            mUsedItems += mTarget->usedItems();
        if (mVertexStream)
            mUsedItems += mVertexStream->usedItems();
    }
    return mPipeline.get();
}

void VKCall::execute(VKContext &context, MessagePtrSet &messages, ScriptEngine &scriptEngine)
{
    switch (mCall.callType) {
        case Call::CallType::Draw:
        case Call::CallType::DrawIndexed:
        case Call::CallType::DrawIndirect:
        case Call::CallType::DrawIndexedIndirect:
            executeDraw(context, messages, scriptEngine);
            break;
        case Call::CallType::Compute:
        case Call::CallType::ComputeIndirect:
            executeCompute(context, messages, scriptEngine);
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
        case Call::CallType::SwapTextures:
            executeSwapTextures(messages);
            break;
        case Call::CallType::SwapBuffers:
            executeSwapBuffers(messages);
            break;
    }
}

int VKCall::evaluateInt(ScriptEngine &scriptEngine, const QString &expression)
{
    return scriptEngine.evaluateInt(expression, mCall.id, mMessages);
}

uint32_t VKCall::evaluateUInt(ScriptEngine &scriptEngine, const QString &expression)
{
    return static_cast<uint32_t>(scriptEngine.evaluateInt(expression, mCall.id, mMessages));
}

void VKCall::executeDraw(VKContext &context, MessagePtrSet &messages, 
    ScriptEngine &scriptEngine)
{
    if (!mTarget) {
        messages += MessageList::insert(
            mCall.id, MessageType::TargetNotAssigned);
        return;
    }

    context.commandRecorder = context.device.createCommandRecorder();
    auto guard = qScopeGuard([&] { context.commandRecorder.reset(); });

    auto primitiveOptions = mTarget->getPrimitiveOptions();
    //primitiveOptions.primitiveRestart = true;
    primitiveOptions.topology = toKDGpu(mCall.primitiveType);
    primitiveOptions.patchControlPoints = evaluateUInt(scriptEngine, mCall.patchVertices);

    if (!mPipeline || !mPipeline->createGraphics(context, primitiveOptions))
        return;

    mPipeline->updateDefaultUniformBlock(context, scriptEngine);

    auto renderPass = mPipeline->beginRenderPass(context);
    if (!renderPass.isValid())
        return;

    if (mIndexBuffer) {
        const auto indicesOffset = evaluateUInt(scriptEngine, mIndicesOffset);
        renderPass.setIndexBuffer(
            mIndexBuffer->getReadOnlyBuffer(context),
            indicesOffset, 
            mIndexType);
    }

    const auto count = evaluateUInt(scriptEngine, mCall.count);
    const auto instanceCount = evaluateUInt(scriptEngine, mCall.instanceCount);
    const auto first = evaluateUInt(scriptEngine, mCall.first);
    const auto firstInstance = evaluateUInt(scriptEngine, mCall.baseInstance);
    const auto baseVertex = evaluateInt(scriptEngine, mCall.baseVertex);
    const auto drawCount = evaluateUInt(scriptEngine, mCall.drawCount);
    const auto indirectOffset = evaluateUInt(scriptEngine, mIndirectOffset);

    if (mCall.callType == Call::CallType::Draw) {
        renderPass.draw({
            .vertexCount = count, 
            .instanceCount = instanceCount, 
            .firstVertex = first,
            .firstInstance = firstInstance,
        });
    }
    else if (mCall.callType == Call::CallType::DrawIndexed) {
        renderPass.drawIndexed({
            .indexCount = count, 
            .instanceCount = instanceCount, 
            .firstIndex = first,
            .vertexOffset = baseVertex,
            .firstInstance = firstInstance,
        });
    }
    else if (mCall.callType == Call::CallType::DrawIndirect) {
        renderPass.drawIndirect({
            .buffer = mIndirectBuffer->getReadOnlyBuffer(context),
            .offset = indirectOffset,
            .drawCount = drawCount,
            .stride = static_cast<uint32_t>(mIndirectStride)
        });
    }
    else if (mCall.callType == Call::CallType::DrawIndexedIndirect) {
        renderPass.drawIndexedIndirect({
            .buffer = mIndirectBuffer->getReadOnlyBuffer(context),
            .offset = indirectOffset,
            .drawCount = drawCount,
            .stride = static_cast<uint32_t>(mIndirectStride)
        });
    }
    renderPass.end();
    context.commandBuffers.push_back(context.commandRecorder->finish());
}

void VKCall::executeCompute(VKContext &context, MessagePtrSet &messages, ScriptEngine &scriptEngine)
{
    context.commandRecorder = context.device.createCommandRecorder();
    auto guard = qScopeGuard([&] { context.commandRecorder.reset(); });

    if (!mPipeline || !mPipeline->createCompute(context))
        return;

    mPipeline->updateDefaultUniformBlock(context, scriptEngine);

    auto computePass = mPipeline->beginComputePass(context);
    computePass.dispatchCompute(KDGpu::ComputeCommand{ 
        .workGroupX = evaluateUInt(scriptEngine, mCall.workGroupsX),
        .workGroupY = evaluateUInt(scriptEngine, mCall.workGroupsY),
        .workGroupZ = evaluateUInt(scriptEngine, mCall.workGroupsZ)
    });
    computePass.end();
    context.commandBuffers.push_back(context.commandRecorder->finish());
}

void VKCall::executeClearTexture(MessagePtrSet &messages)
{
    if (!mTexture) {
        messages += MessageList::insert(
            mCall.id, MessageType::TextureNotAssigned);
        return;
    }

    // TODO:

    mUsedItems += mTexture->usedItems();
}

void VKCall::executeCopyTexture(MessagePtrSet &messages)
{
    if (!mTexture || !mFromTexture) {
        messages += MessageList::insert(
            mCall.id, MessageType::TextureNotAssigned);
        return;
    }
    //auto guard = beginTimerQuery();
    if (!mTexture->copy(*mFromTexture))
        messages += MessageList::insert(
            mCall.id, MessageType::CopyingTextureFailed);

    mUsedItems += mTexture->usedItems();
    mUsedItems += mFromTexture->usedItems();
}

void VKCall::executeClearBuffer(MessagePtrSet &messages)
{
    if (!mBuffer) {
        messages += MessageList::insert(
            mCall.id, MessageType::BufferNotAssigned);
        return;
    }
    //auto guard = beginTimerQuery();
    mBuffer->clear();
    mUsedItems += mBuffer->usedItems();
}

void VKCall::executeCopyBuffer(MessagePtrSet &messages)
{
    if (!mBuffer || !mFromBuffer) {
        messages += MessageList::insert(
            mCall.id, MessageType::BufferNotAssigned);
        return;
    }
    //auto guard = beginTimerQuery();
    mBuffer->copy(*mFromBuffer);
    mUsedItems += mBuffer->usedItems();
    mUsedItems += mFromBuffer->usedItems();
}

void VKCall::executeSwapTextures(MessagePtrSet &messages)
{
    if (!mTexture || !mFromTexture) {
        messages += MessageList::insert(
            mCall.id, MessageType::TextureNotAssigned);
        return;
    }
    if (!mTexture->swap(*mFromTexture))
        messages += MessageList::insert(
            mCall.id, MessageType::SwappingTexturesFailed);
}

void VKCall::executeSwapBuffers(MessagePtrSet &messages)
{
    if (!mBuffer || !mFromBuffer) {
        messages += MessageList::insert(
            mCall.id, MessageType::BufferNotAssigned);
        return;
    }
    if (!mBuffer->swap(*mFromBuffer))
        messages += MessageList::insert(
            mCall.id, MessageType::SwappingBuffersFailed);
}
