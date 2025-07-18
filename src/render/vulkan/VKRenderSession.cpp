#include "VKRenderSession.h"
#include "VKShareSync.h"
#include "VKBuffer.h"
#include "VKCall.h"
#include "VKProgram.h"
#include "VKRenderer.h"
#include "VKStream.h"
#include "VKTarget.h"
#include "VKTexture.h"
#include "VKAccelerationStructure.h"
#include "render/RenderSessionBase_buildCommandQueue.h"
#include <QStack>
#include <deque>
#include <functional>
#include <type_traits>

struct VKRenderSession::CommandQueue
{
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

VKRenderSession::VKRenderSession(RendererPtr renderer, const QString &basePath)
    : RenderSessionBase(std::move(renderer), basePath)
{
}

VKRenderSession::~VKRenderSession()
{
    releaseResources();
}

VKRenderer &VKRenderSession::renderer()
{
    return static_cast<VKRenderer &>(RenderSessionBase::renderer());
}

void VKRenderSession::createCommandQueue()
{
    Q_ASSERT(!mPrevCommandQueue);
    mPrevCommandQueue.swap(mCommandQueue);
    mCommandQueue.reset(new CommandQueue{
        .context =
            VKContext{
                .device = renderer().device(),
                .queue = renderer().queue(),
                .ktxDeviceInfo = renderer().ktxDeviceInfo(),
            },
    });
}

void VKRenderSession::render()
{
    if (!mShareSync)
        mShareSync = std::make_shared<VKShareSync>(renderer().device());

    if (mItemsChanged || mEvaluationType == EvaluationType::Reset) {
        createCommandQueue();
        buildCommandQueue<VKRenderSession>(*mCommandQueue);
    }
    Q_ASSERT(mCommandQueue);
    auto &context = mCommandQueue->context;

    if (mPrevCommandQueue) {
        reuseUnmodifiedItems(*mCommandQueue, *mPrevCommandQueue);
        mPrevCommandQueue.reset();
    }

    mShareSync->beginUpdate();
    context.timestampQueries.clear();

    executeCommandQueue(*mCommandQueue);

    auto submitOptions = KDGpu::SubmitOptions{
        .commandBuffers = std::vector<KDGpu::Handle<KDGpu::CommandBuffer_t>>(
            context.commandBuffers.begin(), context.commandBuffers.end()),
        .signalSemaphores = { mShareSync->usageSemaphore() },
    };
    if (auto &semaphore = mShareSync->usageSemaphore(); semaphore.isValid())
        submitOptions.waitSemaphores.push_back(semaphore);

    context.queue.submit(submitOptions);
    context.queue.waitUntilIdle();
    context.commandBuffers.clear();

    mShareSync->endUpdate();

    downloadModifiedResources(*mCommandQueue);

    if (!updatingPreviewTextures())
        outputTimerQueries(mCommandQueue->context.timestampQueries,
            [](KDGpu::TimestampQueryRecorder &query) {
                return std::chrono::nanoseconds(query.nsInterval(0, 1));
            });
}

void VKRenderSession::finish()
{
    RenderSessionBase::finish();

    if (updatingPreviewTextures() && mCommandQueue)
        updatePreviewTextures(*mCommandQueue, mShareSync);
}

void VKRenderSession::release()
{
    if (mShareSync)
        mShareSync->cleanup();
    mShareSync.reset();
    mCommandQueue.reset();
    mPrevCommandQueue.reset();

    RenderSessionBase::release();
}

quint64 VKRenderSession::getBufferHandle(ItemId itemId)
{
    if (!mCommandQueue)
        createCommandQueue();

    auto &sessionModel = mSessionModelCopy;

    const auto isUsedByAccelerationStructure = [&](const Buffer &buffer) {
        auto isUsed = false;
        for (auto block : buffer.items)
            sessionModel.forEachItem<Geometry>([&](const Geometry &g) {
                isUsed |= (g.vertexBufferBlockId == block->id
                    || g.indexBufferBlockId == block->id
                    || g.transformBufferBlockId == block->id);
            });
        return isUsed;
    };

    const auto addBufferOnce = [&](ItemId bufferId) -> VKBuffer * {
        if (const auto buffer = sessionModel.findItem<Buffer>(bufferId)) {
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
        mUsedItems += buffer->usedItems();
        return buffer->getDeviceAddress(mCommandQueue->context);
    }
    return 0;
}
