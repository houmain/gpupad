#include "VKRenderSession.h"
#include "VKBuffer.h"
#include "VKCall.h"
#include "VKDevice.h"
#include "VKProgram.h"
#include "VKStream.h"
#include "VKTarget.h"
#include "VKTexture.h"
#include "VKAccelerationStructure.h"
#include "render/RenderSessionBase_CommandQueue.h"
#include <algorithm>
#include <QStack>
#include <deque>
#include <functional>
#include <type_traits>

struct VKRenderSession::CommandQueue
{
    ~CommandQueue()
    {
        context.device.waitUntilIdle();
        for (auto &[itemId, texture] : textures)
            texture.release(context.device);
    }

    using Call = VKCall;
    VKContext context;
    std::map<ItemId, VKTexture> textures;
    std::map<ItemId, VKBuffer> buffers;
    std::map<ItemId, VKProgram> programs;
    std::map<ItemId, VKTarget> targets;
    std::map<ItemId, VKStream> vertexStreams;
    std::map<ItemId, VKAccelerationStructure> accelerationStructures;
    std::deque<Command> commands;
    std::vector<VKProgram> failedPrograms;
};

VKRenderSession::VKRenderSession(RendererPtr renderer)
    : RenderSessionBase(std::move(renderer))
{
}

VKRenderSession::~VKRenderSession()
{
    releaseResources();
}

VKDevice &VKRenderSession::vkDevice()
{
    return RenderSessionBase::renderer().device<VKDevice>();
}

void VKRenderSession::createCommandQueue()
{
    Q_ASSERT(!mPrevCommandQueue);
    mPrevCommandQueue.swap(mCommandQueue);

    auto deviceLock = vkDevice().lock();
    mCommandQueue.reset(new CommandQueue{
        .context =
            VKContext{
                .device = deviceLock.device(),
                .queue = deviceLock.queue(),
                .ktxDeviceInfo = deviceLock.ktxDeviceInfo(),
            },
    });
}

std::vector<Duration> VKRenderSession::resetTimeQueries(size_t count)
{
    Q_ASSERT(count <= maxTimeQueries);
    auto durations = std::vector<Duration>();
    durations.reserve(count);
    for (auto i = 0u; i < count; ++i)
        durations.push_back(std::chrono::nanoseconds(
            mTimestampQueries.nsInterval(i * 2, i * 2 + 1)));
    return durations;
}

std::shared_ptr<void> VKRenderSession::beginTimeQuery(size_t index)
{
    Q_ASSERT(index < maxTimeQueries);
    mTimestampQueries.writeTimestamp(KDGpu::PipelineStageFlagBit::TopOfPipeBit);

    return std::shared_ptr<void>(nullptr, [this](void *) {
        mTimestampQueries.writeTimestamp(
            KDGpu::PipelineStageFlagBit::BottomOfPipeBit);
    });
}

void VKRenderSession::render()
{
    auto deviceLock = vkDevice().lock();
    if (itemsChanged() || evaluationType() == EvaluationType::Reset) {
        createCommandQueue();
        buildCommandQueue<VKRenderSession>(*mCommandQueue);
    }
    Q_ASSERT(mCommandQueue);
    auto &context = mCommandQueue->context;

    if (mPrevCommandQueue) {
        reuseUnmodifiedItems(*mCommandQueue, *mPrevCommandQueue);
        mPrevCommandQueue.reset();
    }

    context.commandRecorder = context.device.createCommandRecorder();
    mTimestampQueries =
        mCommandQueue->context.commandRecorder->beginTimestampRecording(
            { .queryCount = maxTimeQueries * 2 });

    executeCommandQueue(*mCommandQueue);

    beginDownloadModifiedResources(*mCommandQueue);

    context.commandBuffers.push_back(context.commandRecorder->finish());
    context.commandRecorder.reset();

    auto submitOptions = KDGpu::SubmitOptions{
        .commandBuffers =
            std::vector<KDGpu::RequiredHandle<KDGpu::CommandBuffer_t>>(
                context.commandBuffers.begin(), context.commandBuffers.end()),
    };
    context.queue.submit(submitOptions);
    context.queue.waitUntilIdle();

    obtainTimeQueryResults();
    context.commandBuffers.clear();
    context.stagingBuffers.clear();
}

void VKRenderSession::finish()
{
    auto deviceLock = vkDevice().lock();
    if (mCommandQueue)
        finishCommandQueue(*mCommandQueue);
}

void VKRenderSession::release()
{
    auto deviceLock = vkDevice().lock();
    mCommandQueue.reset();
    mPrevCommandQueue.reset();

    RenderSessionBase::release();
}

quint64 VKRenderSession::getBufferHandle(ItemId itemId)
{
    auto deviceLock = vkDevice().lock();
    if (!mCommandQueue)
        createCommandQueue();

    const auto isUsedByAccelerationStructure = [&](const Buffer &buffer) {
        auto isUsed = false;
        for (auto block : buffer.items)
            sessionModelCopy().forEachItem<Geometry>([&](const Geometry &g) {
                isUsed |= (g.vertexBufferBlockId == block->id
                    || g.indexBufferBlockId == block->id
                    || g.transformBufferBlockId == block->id);
            });
        return isUsed;
    };

    const auto addBufferOnce = [&](ItemId bufferId) -> VKBuffer * {
        if (const auto buffer = sessionModelCopy().findItem<Buffer>(bufferId)) {
            // ensure that potential current buffer is really getting reused
            if (const auto it = mCommandQueue->buffers.find(buffer->id);
                it != mCommandQueue->buffers.end()
                && it->second != VKBuffer(*buffer, *this))
                mCommandQueue->buffers.erase(it);

            if (auto vkBuffer =
                    addOnce(mCommandQueue->buffers, buffer, *this)) {

                // ensure it is not recreated by usage update
                if (isUsedByAccelerationStructure(*buffer))
                    vkBuffer->addUsage(KDGpu::BufferUsageFlagBits::
                            AccelerationStructureBuildInputReadOnlyBit);

                return vkBuffer;
            }
        }
        return nullptr;
    };

    if (auto buffer = addBufferOnce(itemId)) {
        addUsedItems(buffer->usedItems());
        return buffer->getDeviceAddress(mCommandQueue->context);
    }
    return 0;
}
