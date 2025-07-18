
#include "VKCall.h"
#include "VKBuffer.h"
#include "VKProgram.h"
#include "VKStream.h"
#include "VKTarget.h"
#include "VKTexture.h"
#include <QScopeGuard>
#include <cmath>

VKCall::VKCall(const Call &call, const Session &session)
    : mCall(call)
    , mKind(getKind(mCall))
    , mSession(session)
{
    mUsedItems += session.id;
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
    const auto indexType = getKDIndexType(indexSize);
    if (!indexType || !indexSize) {
        mMessages += MessageList::insert(block.id,
            MessageType::InvalidIndexType,
            QStringLiteral("%1 bytes").arg(indexSize));
        return;
    }

    mIndexType = *indexType;
    mIndexBuffer = indices;
    mIndicesOffset = block.offset;
    mIndicesRowCount = block.rowCount;
    mIndicesPerRow = getBlockStride(block) / indexSize;
}

void VKCall::setIndirectBuffer(VKBuffer *commands, const Block &block)
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

void VKCall::setAccelerationStructure(VKAccelerationStructure *accelStruct)
{
    mAccelerationStructure = accelStruct;
}

bool VKCall::validateShaderTypes()
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

void VKCall::execute(VKContext &context, Bindings &&bindings,
    MessagePtrSet &messages, ScriptEngine &scriptEngine)
{
    if (mKind.trace && !context.features().rayTracingPipeline) {
        mMessages +=
            MessageList::insert(mCall.id, MessageType::RayTracingNotAvailable);
        return;
    }

    if (mKind.mesh && !context.features().meshShader) {
        mMessages +=
            MessageList::insert(mCall.id, MessageType::MeshShadersNotAvailable);
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

        mPipeline = std::make_unique<VKPipeline>(mCall.id, mProgram);
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

    recorder.writeTimestamp(KDGpu::PipelineStageFlagBit::BottomOfPipeBit);
    context.commandBuffers.push_back(context.commandRecorder->finish());
}

int VKCall::getMaxElementCount(ScriptEngine &scriptEngine)
{
    if (mKind.indexed)
        return scriptEngine.evaluateInt(mIndicesRowCount, mCall.id)
            * mIndicesPerRow;
    if (mVertexStream)
        return mVertexStream->maxElementCount();
    return -1;
}

void VKCall::executeDraw(VKContext &context, MessagePtrSet &messages,
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

    auto primitiveOptions = mTarget->getPrimitiveOptions();
    //primitiveOptions.primitiveRestart = true;
    primitiveOptions.topology = toKDGpu(mCall.primitiveType);
    primitiveOptions.patchControlPoints =
        scriptEngine.evaluateUInt(mCall.patchVertices, mCall.id);

    if (!mPipeline
        || !mPipeline->createGraphics(context, primitiveOptions, mTarget,
            mVertexStream))
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

    auto renderPass =
        mPipeline->beginRenderPass(context, mSession.flipViewport);
    if (!renderPass.isValid())
        return;

    if (!mPipeline->updatePushConstants(renderPass, scriptEngine))
        return;

    if (mIndexBuffer) {
        const auto indicesOffset =
            scriptEngine.evaluateUInt(mIndicesOffset, mCall.id);
        renderPass.setIndexBuffer(mIndexBuffer->buffer(), indicesOffset,
            mIndexType);
    }

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
            .workGroupX =
                scriptEngine.evaluateUInt(mCall.workGroupsX, mCall.id),
            .workGroupY =
                scriptEngine.evaluateUInt(mCall.workGroupsY, mCall.id),
            .workGroupZ =
                scriptEngine.evaluateUInt(mCall.workGroupsZ, mCall.id),
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

    if (!mPipeline->updatePushConstants(computePass, scriptEngine))
        return;

    computePass.dispatchCompute(KDGpu::ComputeCommand{
        .workGroupX = scriptEngine.evaluateUInt(mCall.workGroupsX, mCall.id),
        .workGroupY = scriptEngine.evaluateUInt(mCall.workGroupsY, mCall.id),
        .workGroupZ = scriptEngine.evaluateUInt(mCall.workGroupsZ, mCall.id),
    });
    computePass.end();
    mUsedItems += mPipeline->usedItems();
}

void VKCall::executeTraceRays(VKContext &context, MessagePtrSet &messages,
    ScriptEngine &scriptEngine)
{
    if (!mPipeline
        || !mPipeline->createRayTracing(context, mAccelerationStructure))
        return;

    const auto canRender = mPipeline->updateBindings(context, scriptEngine);
    if (!canRender) {
        mUsedItems += mPipeline->usedItems();
        return;
    }

    auto rayTracingPass = mPipeline->beginRayTracingPass(context);
    if (!rayTracingPass.isValid())
        return;

    if (!mPipeline->updatePushConstants(rayTracingPass, scriptEngine))
        return;

    const auto &sbt = mPipeline->rayTracingShaderBindingTable();
    rayTracingPass.traceRays(KDGpu::RayTracingCommand{
        .raygenShaderBindingTable = sbt.rayGenShaderRegion(),
        .missShaderBindingTable = sbt.missShaderRegion(),
        .hitShaderBindingTable = sbt.hitShaderRegion(),
        .callableShaderBindingTable = {},
        .extent = {
            .width = scriptEngine.evaluateUInt(mCall.workGroupsX, mCall.id),
            .height = scriptEngine.evaluateUInt(mCall.workGroupsY, mCall.id),
            .depth = scriptEngine.evaluateUInt(mCall.workGroupsZ, mCall.id),
        },
    });

    rayTracingPass.end();
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
