#include "D3DRenderSession.h"
#include "D3DShareSync.h"
#include "D3DBuffer.h"
#include "D3DCall.h"
#include "D3DProgram.h"
#include "D3DRenderer.h"
#include "D3DStream.h"
#include "D3DTarget.h"
#include "D3DTexture.h"
#include "D3DAccelerationStructure.h"
#include "render/RenderSessionBase_CommandQueue.h"
#include <QStack>
#include <deque>
#include <functional>
#include <type_traits>

struct D3DRenderSession::CommandQueue
{
    using Call = D3DCall;
    D3DContext context;
    std::map<ItemId, D3DTexture> textures;
    std::map<ItemId, D3DBuffer> buffers;
    std::map<ItemId, D3DProgram> programs;
    std::map<ItemId, D3DTarget> targets;
    std::map<ItemId, D3DStream> vertexStreams;
    std::map<ItemId, D3DAccelerationStructure> accelerationStructures;
    std::deque<Command> commands;
    std::vector<D3DProgram> failedPrograms;
};

D3DRenderSession::D3DRenderSession(RendererPtr renderer,
    const QString &basePath)
    : RenderSessionBase(std::move(renderer), basePath)
{
}

D3DRenderSession::~D3DRenderSession()
{
    releaseResources();
}

D3DRenderer &D3DRenderSession::renderer()
{
    return static_cast<D3DRenderer &>(RenderSessionBase::renderer());
}

void D3DRenderSession::createCommandQueue()
{
    Q_ASSERT(!mPrevCommandQueue);
    mPrevCommandQueue.swap(mCommandQueue);

    mCommandQueue.reset(new CommandQueue{
        .context =
            D3DContext{
                .device = renderer().device(),
                .queue = renderer().queue(),
                .renderTargetHelper = renderer().renderTargetHelper(),
                .descriptorSize =
                    renderer().device().GetDescriptorHandleIncrementSize(
                        D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV),
            },
    });

    auto &context = mCommandQueue->context;
    AssertIfFailed(context.device.CreateCommandAllocator(
        D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&mCommandAllocator)));

    AssertIfFailed(context.device.CreateCommandList(0,
        D3D12_COMMAND_LIST_TYPE_DIRECT, mCommandAllocator.Get(), nullptr,
        IID_PPV_ARGS(&context.graphicsCommandList)));
    AssertIfFailed(context.graphicsCommandList->Close());

    if (!mTimeQueryHeap) {
        const auto queryHeapDesc = D3D12_QUERY_HEAP_DESC{
            .Type = D3D12_QUERY_HEAP_TYPE_TIMESTAMP,
            .Count = maxTimeQueries * 2,
        };
        context.device.CreateQueryHeap(&queryHeapDesc,
            IID_PPV_ARGS(&mTimeQueryHeap));

        const auto stagingHeapProperties =
            CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_READBACK);
        const auto stagingBufferDesc = CD3DX12_RESOURCE_DESC::Buffer(
            sizeof(uint64_t) * queryHeapDesc.Count);
        AssertIfFailed(context.device.CreateCommittedResource(
            &stagingHeapProperties, D3D12_HEAP_FLAG_NONE, &stagingBufferDesc,
            D3D12_RESOURCE_STATE_COPY_DEST, nullptr,
            IID_PPV_ARGS(&mTimeQueryResolveBuffer)));
    }
}

std::vector<Duration> D3DRenderSession::resetTimeQueries(size_t count)
{
    Q_ASSERT(count <= maxTimeQueries);
    auto mappedData = std::add_pointer_t<void>{};
    AssertIfFailed(mTimeQueryResolveBuffer->Map(0, nullptr, &mappedData));
    const auto *timestamps = static_cast<const uint64_t *>(mappedData);
    if (!timestamps)
        return {};

    auto durations = std::vector<Duration>();
    durations.reserve(count);
    for (auto i = 0u; i < count; ++i)
        durations.push_back(std::chrono::nanoseconds(
            timestamps[i * 2 + 1] - timestamps[i * 2]));

    mTimeQueryResolveBuffer->Unmap(0, nullptr);
    return durations;
}

std::shared_ptr<void> D3DRenderSession::beginTimeQuery(size_t index)
{
    Q_ASSERT(index < maxTimeQueries);
    mCommandQueue->context.graphicsCommandList->EndQuery(mTimeQueryHeap.Get(),
        D3D12_QUERY_TYPE_TIMESTAMP, static_cast<UINT>(index * 2));

    return std::shared_ptr<void>(nullptr, [this, index](void *) {
        mCommandQueue->context.graphicsCommandList->EndQuery(
            mTimeQueryHeap.Get(), D3D12_QUERY_TYPE_TIMESTAMP,
            static_cast<UINT>(index * 2 + 1));
    });
}

void D3DRenderSession::render()
{
    if (!mShareSync)
        mShareSync = std::make_shared<D3DShareSync>(renderer().device());

    if (itemsChanged() || evaluationType() == EvaluationType::Reset) {
        createCommandQueue();
        buildCommandQueue<D3DRenderSession>(*mCommandQueue);
    }
    Q_ASSERT(mCommandQueue);
    auto &context = mCommandQueue->context;

    if (mPrevCommandQueue) {
        reuseUnmodifiedItems(*mCommandQueue, *mPrevCommandQueue);
        mPrevCommandQueue.reset();
    }

    mShareSync->beginUpdate();

    AssertIfFailed(
        context.graphicsCommandList->Reset(mCommandAllocator.Get(), nullptr));

    executeCommandQueue(*mCommandQueue);

    beginDownloadModifiedResources(*mCommandQueue);

    context.graphicsCommandList->ResolveQueryData(mTimeQueryHeap.Get(),
        D3D12_QUERY_TYPE_TIMESTAMP, 0, static_cast<UINT>(timeQueryCount() * 2),
        mTimeQueryResolveBuffer.Get(), 0);

    AssertIfFailed(context.graphicsCommandList->Close());
    auto commandLists =
        std::array<ID3D12CommandList *, 1>{ context.graphicsCommandList.Get() };
    context.queue.ExecuteCommandLists(static_cast<UINT>(commandLists.size()),
        commandLists.data());

    if (!mFence)
        AssertIfFailed(this->renderer().device().CreateFence(0,
            D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&mFence)));

    mFenceValue++;
    AssertIfFailed(context.queue.Signal(mFence.Get(), mFenceValue));
    if (mFence->GetCompletedValue() < mFenceValue)
        if (auto event = CreateEventExA(nullptr, 0, 0, EVENT_ALL_ACCESS)) {
            AssertIfFailed(mFence->SetEventOnCompletion(mFenceValue, event));
            WaitForSingleObject(event, INFINITE);
            CloseHandle(event);
        }

    context.stagingBuffers.clear();
    obtainTimeQueryResults();

    mShareSync->endUpdate();
}

void D3DRenderSession::finish()
{
    if (mCommandQueue)
        finishCommandQueue(*mCommandQueue, mShareSync);
}

void D3DRenderSession::release()
{
    mCommandAllocator.Reset();
    mFence.Reset();
    if (mShareSync)
        mShareSync->cleanup();
    mShareSync.reset();
    mCommandQueue.reset();
    mPrevCommandQueue.reset();

    RenderSessionBase::release();
}

quint64 D3DRenderSession::getBufferHandle(ItemId itemId)
{
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

    const auto addBufferOnce = [&](ItemId bufferId) -> D3DBuffer * {
        if (const auto buffer = sessionModelCopy().findItem<Buffer>(bufferId)) {
            // ensure that potential current buffer is really getting reused
            if (const auto it = mCommandQueue->buffers.find(buffer->id);
                it != mCommandQueue->buffers.end()
                && it->second != D3DBuffer(*buffer, *this))
                mCommandQueue->buffers.erase(it);

            return addOnce(mCommandQueue->buffers, buffer, *this);
        }
        return nullptr;
    };

    if (auto buffer = addBufferOnce(itemId)) {
        addUsedItems(buffer->usedItems());
        return buffer->getDeviceAddress();
    }
    return 0;
}
