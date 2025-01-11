
#include "VKCall.h"
#include "VKBuffer.h"
#include "VKProgram.h"
#include "VKStream.h"
#include "VKTarget.h"
#include "VKTexture.h"
#include <QScopeGuard>
#include <cmath>

VKCall::VKCall(const Call &call) : mCall(call) { }

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
    mIndexType = (getBlockStride(block) == 2 ? KDGpu::IndexType::Uint16
                                             : KDGpu::IndexType::Uint32);
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

VKPipeline *VKCall::getPipeline(VKContext &context)
{
    if (!mPipeline && mProgram) {
        mPipeline = std::make_unique<VKPipeline>(mCall.id, mProgram, mTarget,
            mVertexStream);

        mProgram->link(context.device);
        mUsedItems += mProgram->usedItems();

        if (mTarget)
            mUsedItems += mTarget->usedItems();
        if (mVertexStream)
            mUsedItems += mVertexStream->usedItems();
    }
    return mPipeline.get();
}

void VKCall::execute(VKContext &context, MessagePtrSet &messages,
    ScriptEngine &scriptEngine)
{
    const auto kind = getKind(mCall);

    if ((kind.draw || kind.compute) && !mProgram) {
        messages +=
            MessageList::insert(mCall.id, MessageType::ProgramNotAssigned);
        return;
    }

    if (kind.draw && !mTarget) {
        messages +=
            MessageList::insert(mCall.id, MessageType::TargetNotAssigned);
        return;
    }

    if (kind.indexed && !mIndexBuffer) {
        messages +=
            MessageList::insert(mCall.id, MessageType::IndexBufferNotAssigned);
        return;
    }

    if (kind.indirect && !mIndirectBuffer) {
        messages += MessageList::insert(mCall.id,
            MessageType::IndirectBufferNotAssigned);
        return;
    }

    context.commandRecorder = context.device.createCommandRecorder();
    auto guard = qScopeGuard([&] { context.commandRecorder.reset(); });

    auto &recorder = context.timestampQueries[mCall.id];
    recorder =
        context.commandRecorder->beginTimestampRecording({ .queryCount = 2 });
    recorder.writeTimestamp(KDGpu::PipelineStageFlagBit::TopOfPipeBit);

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

    recorder.writeTimestamp(KDGpu::PipelineStageFlagBit::BottomOfPipeBit);
    context.commandBuffers.push_back(context.commandRecorder->finish());
}

int VKCall::evaluateInt(ScriptEngine &scriptEngine, const QString &expression)
{
    return scriptEngine.evaluateInt(expression, mCall.id, mMessages);
}

uint32_t VKCall::evaluateUInt(ScriptEngine &scriptEngine,
    const QString &expression)
{
    return static_cast<uint32_t>(
        scriptEngine.evaluateInt(expression, mCall.id, mMessages));
}

void VKCall::executeDraw(VKContext &context, MessagePtrSet &messages,
    ScriptEngine &scriptEngine)
{
    auto primitiveOptions = mTarget->getPrimitiveOptions();
    //primitiveOptions.primitiveRestart = true;
    primitiveOptions.topology = toKDGpu(mCall.primitiveType);
    primitiveOptions.patchControlPoints =
        evaluateUInt(scriptEngine, mCall.patchVertices);

    if (!mPipeline || !mPipeline->createGraphics(context, primitiveOptions))
        return;

    const auto canRender = mPipeline->updateBindings(context, scriptEngine);
    if (!canRender) {
        mUsedItems += mPipeline->usedItems();
        return;
    }

    if (mIndirectBuffer)
        mIndirectBuffer->prepareIndirectBuffer(context);

    if (mIndexBuffer)
        mIndexBuffer->prepareIndexBuffer(context);

    auto renderPass = mPipeline->beginRenderPass(context);
    if (!renderPass.isValid())
        return;

    mPipeline->updatePushConstants(renderPass, scriptEngine);

    if (mIndexBuffer) {
        const auto indicesOffset = evaluateUInt(scriptEngine, mIndicesOffset);
        renderPass.setIndexBuffer(mIndexBuffer->buffer(), indicesOffset,
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
    } else if (mCall.callType == Call::CallType::DrawIndexed) {
        renderPass.drawIndexed({
            .indexCount = count,
            .instanceCount = instanceCount,
            .firstIndex = first,
            .vertexOffset = baseVertex,
            .firstInstance = firstInstance,
        });
    } else if (mCall.callType == Call::CallType::DrawIndirect) {
        renderPass.drawIndirect({
            .buffer = mIndirectBuffer->buffer(),
            .offset = indirectOffset,
            .drawCount = drawCount,
            .stride = static_cast<uint32_t>(mIndirectStride),
        });
    } else if (mCall.callType == Call::CallType::DrawIndexedIndirect) {
        renderPass.drawIndexedIndirect({
            .buffer = mIndirectBuffer->buffer(),
            .offset = indirectOffset,
            .drawCount = drawCount,
            .stride = static_cast<uint32_t>(mIndirectStride),
        });
    } else if (mCall.callType == Call::CallType::DrawMeshTasks) {
        renderPass.drawMeshTasks({
            .workGroupX = evaluateUInt(scriptEngine, mCall.workGroupsX),
            .workGroupY = evaluateUInt(scriptEngine, mCall.workGroupsY),
            .workGroupZ = evaluateUInt(scriptEngine, mCall.workGroupsZ),
        });
    } else if (mCall.callType == Call::CallType::DrawMeshTasksIndirect) {
        renderPass.drawMeshTasksIndirect({
            .buffer = mIndirectBuffer->buffer(),
            .offset = indirectOffset,
            .drawCount = drawCount,
            .stride = static_cast<uint32_t>(mIndirectStride),
        });
    }
    renderPass.end();
    mUsedItems += mPipeline->usedItems();
}

void VKCall::executeCompute(VKContext &context, MessagePtrSet &messages,
    ScriptEngine &scriptEngine)
{
    if (!mPipeline || !mPipeline->createCompute(context))
        return;

    const auto canRender = mPipeline->updateBindings(context, scriptEngine);
    if (!canRender) {
        mUsedItems += mPipeline->usedItems();
        return;
    }

    auto computePass = mPipeline->beginComputePass(context);
    if (!computePass.isValid())
        return;

    mPipeline->updatePushConstants(computePass, scriptEngine);

    computePass.dispatchCompute(KDGpu::ComputeCommand{
        .workGroupX = evaluateUInt(scriptEngine, mCall.workGroupsX),
        .workGroupY = evaluateUInt(scriptEngine, mCall.workGroupsY),
        .workGroupZ = evaluateUInt(scriptEngine, mCall.workGroupsZ),
    });
    computePass.end();
    mUsedItems += mPipeline->usedItems();
}

void VKCall::executeClearTexture(VKContext &context, MessagePtrSet &messages)
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

    //auto guard = beginTimerQuery();
    if (!mTexture->clear(context, color, mCall.clearDepth, mCall.clearStencil))
        messages +=
            MessageList::insert(mCall.id, MessageType::ClearingTextureFailed);

    mUsedItems += mTexture->usedItems();
}

void VKCall::executeCopyTexture(VKContext &context, MessagePtrSet &messages)
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

void VKCall::executeClearBuffer(VKContext &context, MessagePtrSet &messages)
{
    if (!mBuffer) {
        messages +=
            MessageList::insert(mCall.id, MessageType::BufferNotAssigned);
        return;
    }
    mBuffer->clear(context);
    mUsedItems += mBuffer->usedItems();
}

void VKCall::executeCopyBuffer(VKContext &context, MessagePtrSet &messages)
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

void VKCall::executeSwapTextures(MessagePtrSet &messages)
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

void VKCall::executeSwapBuffers(MessagePtrSet &messages)
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
