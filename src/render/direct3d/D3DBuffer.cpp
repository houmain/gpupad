#include "D3DBuffer.h"
#include "Singletons.h"
#include <QScopeGuard>

D3DBuffer::D3DBuffer(const Buffer &buffer, D3DRenderSession &renderSession)
    : BufferBase(buffer, renderSession.getBufferSize(buffer))
{
    mUsedItems += buffer.id;
    for (const auto item : buffer.items)
        if (auto block = static_cast<const Block *>(item)) {
            mUsedItems += block->id;
            for (const auto item : block->items)
                if (auto field = static_cast<const Block *>(item))
                    mUsedItems += field->id;
        }
}

D3DBuffer::D3DBuffer(int size) : BufferBase(size) { }

void D3DBuffer::clear(D3DContext &context)
{
    Q_ASSERT(!"not implemented");
}

void D3DBuffer::copy(D3DContext &context, D3DBuffer &source)
{
    Q_ASSERT(!"not implemented");
}

bool D3DBuffer::swap(D3DBuffer &other)
{
    if (mSize != other.mSize)
        return false;
    mData.swap(other.mData);
    std::swap(mResource, other.mResource);
    std::swap(mSystemCopyModified, other.mSystemCopyModified);
    std::swap(mDeviceCopyModified, other.mDeviceCopyModified);
    return true;
}

void D3DBuffer::updateReadOnlyBuffer(D3DContext &context)
{
    reload();
    createBuffer(context);
    upload(context);
}

void D3DBuffer::updateReadWriteBuffer(D3DContext &context)
{
    reload();
    createBuffer(context);
    upload(context);
    mDeviceCopyModified = true;
}

void D3DBuffer::reload()
{
    auto prevData = mData;
    if (!mFileName.isEmpty())
        if (!Singletons::fileCache().getBinary(mFileName, &mData))
            if (!FileDialog::isEmptyOrUntitled(mFileName))
                mMessages += MessageList::insert(mItemId,
                    MessageType::LoadingFileFailed, mFileName);

    if (mSize > mData.size())
        mData.append(QByteArray(mSize - mData.size(), 0));

    mSystemCopyModified |= !mData.isSharedWith(prevData);
}

void D3DBuffer::createBuffer(D3DContext &context)
{
    if (mResource)
        return;

    const auto resourceDesc = CD3DX12_RESOURCE_DESC::Buffer(alignedSize());
    const auto heapProperties =
        CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT);
    AssertIfFailed(context.device.CreateCommittedResource(&heapProperties,
        D3D12_HEAP_FLAG_NONE, &resourceDesc, D3D12_RESOURCE_STATE_COMMON,
        nullptr, IID_PPV_ARGS(&mResource)));
}

void D3DBuffer::upload(D3DContext &context)
{
    if (!mSystemCopyModified)
        return;

    const auto size = static_cast<UINT64>(mSize);

    const auto stagingHeapProperties =
        CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD);
    const auto stagingBufferDesc = CD3DX12_RESOURCE_DESC::Buffer(size);
    auto stagingBuffer = ComPtr<ID3D12Resource>();
    AssertIfFailed(context.device.CreateCommittedResource(
        &stagingHeapProperties, D3D12_HEAP_FLAG_NONE, &stagingBufferDesc,
        D3D12_RESOURCE_STATE_COPY_SOURCE, nullptr,
        IID_PPV_ARGS(&stagingBuffer)));

    auto mappedData = std::add_pointer_t<void>{};
    AssertIfFailed(stagingBuffer->Map(0, nullptr, &mappedData));
    std::memcpy(mappedData, mData.constData(), size);
    stagingBuffer->Unmap(0, nullptr);

    resourceBarrier(context, D3D12_RESOURCE_STATE_COPY_DEST);

    context.graphicsCommandList->CopyBufferRegion(mResource.Get(), 0,
        stagingBuffer.Get(), 0, size);

    context.stagingBuffers.push_back(std::move(stagingBuffer));

    mSystemCopyModified = mDeviceCopyModified = false;
}

void D3DBuffer::beginDownload(D3DContext &context, bool checkModification)
{
    if (!mDeviceCopyModified)
        return;

    resourceBarrier(context, D3D12_RESOURCE_STATE_COPY_SOURCE);

    if (!mDownloadBuffer) {
        const auto heapProperties = D3D12_HEAP_PROPERTIES{
            .Type = D3D12_HEAP_TYPE::D3D12_HEAP_TYPE_READBACK,
            .CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN,
            .MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN,
            .VisibleNodeMask = 1,
        };

        const auto bufferDesc = D3D12_RESOURCE_DESC{
            .Dimension = D3D12_RESOURCE_DIMENSION_BUFFER,
            .Width = static_cast<UINT64>(mSize),
            .Height = 1,
            .DepthOrArraySize = 1,
            .MipLevels = 1,
            .Format = DXGI_FORMAT_UNKNOWN,
            .SampleDesc = { .Count = 1 },
            .Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR,
            .Flags = D3D12_RESOURCE_FLAG_NONE,
        };
        AssertIfFailed(context.device.CreateCommittedResource(&heapProperties,
            D3D12_HEAP_FLAG_NONE, &bufferDesc, D3D12_RESOURCE_STATE_COPY_DEST,
            nullptr, IID_PPV_ARGS(&mDownloadBuffer)));
    }

    context.graphicsCommandList->CopyBufferRegion(mDownloadBuffer.Get(), 0,
        mResource.Get(), 0, static_cast<UINT64>(mSize));

    mSystemCopyModified = mDeviceCopyModified = false;
    mCheckModification = checkModification;
    mDownloading = true;
}

bool D3DBuffer::finishDownload()
{
    auto modified = false;
    if (mDownloading) {
        mDownloading = false;
        auto mappedData = std::add_pointer_t<void>{};
        AssertIfFailed(mDownloadBuffer->Map(0, nullptr, &mappedData));
        if (!mCheckModification
            || std::memcmp(mData.data(), mappedData, mData.size()) != 0) {
            std::memcpy(mData.data(), mappedData, mData.size());
            modified = true;
        }
        mDownloadBuffer->Unmap(0, nullptr);
    }
    return modified;
}

void D3DBuffer::resourceBarrier(D3DContext &context,
    D3D12_RESOURCE_STATES state)
{
    if (state == mCurrentState) {
        if (state == D3D12_RESOURCE_STATE_UNORDERED_ACCESS) {
            const auto barrier = CD3DX12_RESOURCE_BARRIER::UAV(resource());
            context.graphicsCommandList->ResourceBarrier(1, &barrier);
        }
    } else {
        const auto transition = CD3DX12_RESOURCE_BARRIER::Transition(resource(),
            mCurrentState, state);
        context.graphicsCommandList->ResourceBarrier(1, &transition);
        mCurrentState = state;
    }
}

void D3DBuffer::prepareVertexBuffer(D3DContext &context)
{
    updateReadOnlyBuffer(context);
    resourceBarrier(context, D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER);
}

void D3DBuffer::prepareIndexBuffer(D3DContext &context)
{
    updateReadOnlyBuffer(context);
    resourceBarrier(context, D3D12_RESOURCE_STATE_INDEX_BUFFER);
}

void D3DBuffer::prepareConstantBufferView(D3DContext &context)
{
    updateReadOnlyBuffer(context);
    resourceBarrier(context, D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER);
}

D3D12_GPU_VIRTUAL_ADDRESS D3DBuffer::getDeviceAddress()
{
    return mResource->GetGPUVirtualAddress();
}
