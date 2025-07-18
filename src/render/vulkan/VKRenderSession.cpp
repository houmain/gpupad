#include "VKRenderSession.h"
#include "Singletons.h"
#include "VKShareSync.h"
#include "VKBuffer.h"
#include "VKCall.h"
#include "VKProgram.h"
#include "VKRenderer.h"
#include "VKStream.h"
#include "VKTarget.h"
#include "VKTexture.h"
#include "VKAccelerationStructure.h"
#include "editors/EditorManager.h"
#include "editors/texture/TextureEditor.h"
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

    if (mPrevCommandQueue) {
        reuseUnmodifiedItems(*mCommandQueue, *mPrevCommandQueue);
        mPrevCommandQueue.reset();
    }
    executeCommandQueue();
    downloadModifiedResources();
    if (!updatingPreviewTextures())
        outputTimerQueries();
}

void VKRenderSession::executeCommandQueue()
{
    mShareSync->beginUpdate();

    auto state = BindingState{};
    mCommandQueue->context.timestampQueries.clear();

    mNextCommandQueueIndex = 0;
    while (mNextCommandQueueIndex < mCommandQueue->commands.size()) {
        const auto index = mNextCommandQueueIndex++;
        // executing command might call setNextCommandQueueIndex
        mCommandQueue->commands[index](state);
    }

    auto submitOptions = KDGpu::SubmitOptions{
        .commandBuffers = std::vector<KDGpu::Handle<KDGpu::CommandBuffer_t>>(
            mCommandQueue->context.commandBuffers.begin(),
            mCommandQueue->context.commandBuffers.end()),
        .signalSemaphores = { mShareSync->usageSemaphore() },
    };
    if (auto &semaphore = mShareSync->usageSemaphore(); semaphore.isValid())
        submitOptions.waitSemaphores.push_back(semaphore);

    mCommandQueue->context.queue.submit(submitOptions);
    mCommandQueue->context.queue.waitUntilIdle();
    mCommandQueue->context.commandBuffers.clear();

    mShareSync->endUpdate();
}

void VKRenderSession::downloadModifiedResources()
{
    for (auto &[itemId, program] : mCommandQueue->programs)
        if (program.printf().isUsed())
            mMessages += program.printf().formatMessages(mCommandQueue->context,
                program.itemId());

    for (auto &[itemId, texture] : mCommandQueue->textures)
        if (!texture.fileName().isEmpty()) {
            texture.updateMipmaps(mCommandQueue->context);
            if (!updatingPreviewTextures()
                && texture.download(mCommandQueue->context))
                mModifiedTextures[texture.itemId()] = texture.data();
        }

    for (auto &[itemId, buffer] : mCommandQueue->buffers)
        if (!buffer.fileName().isEmpty()
            && (mItemsChanged || mEvaluationType != EvaluationType::Steady)
            && buffer.download(mCommandQueue->context,
                mEvaluationType != EvaluationType::Reset))
            mModifiedBuffers[buffer.itemId()] = buffer.data();
}

void VKRenderSession::outputTimerQueries()
{
    mTimerMessages.clear();

    auto total = std::chrono::nanoseconds::zero();
    auto &queries = mCommandQueue->context.timestampQueries;
    for (auto &[itemId, query] : queries) {
        const auto duration = std::chrono::nanoseconds(query.nsInterval(0, 1));
        mTimerMessages += MessageList::insert(itemId, MessageType::CallDuration,
            formatDuration(duration), false);
        total += duration;
    }
    if (queries.size() > 1)
        mTimerMessages += MessageList::insert(0, MessageType::TotalDuration,
            formatDuration(total), false);
}

void VKRenderSession::finish()
{
    RenderSessionBase::finish();

    if (!mCommandQueue)
        return;

    auto &editors = Singletons::editorManager();
    auto &sessionModel = Singletons::sessionModel();

    if (updatingPreviewTextures())
        for (const auto &[itemId, texture] : mCommandQueue->textures)
            if (texture.deviceCopyModified())
                if (auto fileItem =
                        castItem<FileItem>(sessionModel.findItem(itemId)))
                    if (auto editor =
                            editors.getTextureEditor(fileItem->fileName))
                        if (auto handle = texture.getSharedMemoryHandle();
                            handle.handle)
                            editor->updatePreviewTexture(mShareSync, handle,
                                texture.samples());
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
