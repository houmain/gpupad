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
    // see https://asawicki.info/news_1795_secrets_of_direct3d_12_the_behavior_of_clearunorderedaccessviewuintfloat
    mMessages += MessageList::insert(mItemId, MessageType::NotImplemented,
        "Clear Buffer");
}

void D3DBuffer::copy(D3DContext &context, D3DBuffer &source)
{
    resourceBarrier(context, D3D12_RESOURCE_STATE_COPY_DEST);
    source.resourceBarrier(context, D3D12_RESOURCE_STATE_COPY_SOURCE);
    context.graphicsCommandList->CopyResource(resource(), source.resource());
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

void D3DBuffer::initialize(D3DContext &context)
{
    reload();
    upload(context);
}

void D3DBuffer::updateReadOnlyBuffer(D3DContext &context)
{
    reload();
    upload(context);
}

void D3DBuffer::updateReadWriteBuffer(D3DContext &context)
{
    reload();
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

    const auto flags = D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;
    const auto resourceDesc =
        CD3DX12_RESOURCE_DESC::Buffer(alignedSize(), flags);
    const auto heapProperties =
        CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT);
    AssertIfFailed(context.device.CreateCommittedResource(&heapProperties,
        D3D12_HEAP_FLAG_NONE, &resourceDesc, D3D12_RESOURCE_STATE_COMMON,
        nullptr, IID_PPV_ARGS(&mResource)));
}

ComPtr<ID3D12Resource> D3DBuffer::createStagingBuffer(D3DContext &context,
    D3D12_HEAP_TYPE type)
{
    const auto stagingHeapProperties = CD3DX12_HEAP_PROPERTIES(type);
    const auto stagingBufferDesc =
        CD3DX12_RESOURCE_DESC::Buffer(static_cast<UINT64>(mSize));
    const auto initialState = (type == D3D12_HEAP_TYPE_UPLOAD
            ? D3D12_RESOURCE_STATE_COPY_SOURCE
            : D3D12_RESOURCE_STATE_COPY_DEST);
    auto stagingBuffer = ComPtr<ID3D12Resource>();
    AssertIfFailed(context.device.CreateCommittedResource(
        &stagingHeapProperties, D3D12_HEAP_FLAG_NONE, &stagingBufferDesc,
        initialState, nullptr, IID_PPV_ARGS(&stagingBuffer)));

    context.stagingBuffers.push_back(stagingBuffer);
    return stagingBuffer;
}

void D3DBuffer::upload(D3DContext &context, const void *data, size_t size)
{
    createBuffer(context);

    auto stagingBuffer = createStagingBuffer(context, D3D12_HEAP_TYPE_UPLOAD);
    auto mappedData = std::add_pointer_t<void>{};
    AssertIfFailed(stagingBuffer->Map(0, nullptr, &mappedData));
    std::memcpy(mappedData, data, size);
    stagingBuffer->Unmap(0, nullptr);

    resourceBarrier(context, D3D12_RESOURCE_STATE_COPY_DEST);
    context.graphicsCommandList->CopyBufferRegion(mResource.Get(), 0,
        stagingBuffer.Get(), 0, static_cast<UINT64>(size));
}

void D3DBuffer::upload(D3DContext &context)
{
    if (!mSystemCopyModified)
        return;

    upload(context, mData.constData(), mSize);

    mSystemCopyModified = mDeviceCopyModified = false;
}

void D3DBuffer::beginDownload(D3DContext &context, bool checkModification)
{
    if (!mDeviceCopyModified)
        return;

    Q_ASSERT(!mDownloadBuffer);
    mDownloadBuffer = createStagingBuffer(context, D3D12_HEAP_TYPE_READBACK);

    resourceBarrier(context, D3D12_RESOURCE_STATE_COPY_SOURCE);
    context.graphicsCommandList->CopyBufferRegion(mDownloadBuffer.Get(), 0,
        mResource.Get(), 0, static_cast<UINT64>(mSize));

    mSystemCopyModified = mDeviceCopyModified = false;
    mCheckModification = checkModification;
}

bool D3DBuffer::finishDownload()
{
    auto modified = false;
    if (mDownloadBuffer) {
        auto mappedData = std::add_pointer_t<void>{};
        AssertIfFailed(mDownloadBuffer->Map(0, nullptr, &mappedData));
        if (!mCheckModification
            || std::memcmp(mData.data(), mappedData, mData.size()) != 0) {
            std::memcpy(mData.data(), mappedData, mData.size());
            modified = true;
        }
        mDownloadBuffer->Unmap(0, nullptr);
        mDownloadBuffer.Reset();
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

void D3DBuffer::prepareConstantBufferView(D3DContext &context,
    CD3DX12_CPU_DESCRIPTOR_HANDLE &descriptor)
{
    updateReadOnlyBuffer(context);
    resourceBarrier(context, D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER);

    const auto cbvDesc = D3D12_CONSTANT_BUFFER_VIEW_DESC{
        .BufferLocation = getDeviceAddress(),
        .SizeInBytes = alignedSize(),
    };
    context.device.CreateConstantBufferView(&cbvDesc, descriptor);
    descriptor.Offset(1, context.descriptorSize);
}

void D3DBuffer::prepareUnorderedAccessView(D3DContext &context,
    CD3DX12_CPU_DESCRIPTOR_HANDLE &descriptor)
{
    updateReadWriteBuffer(context);
    resourceBarrier(context, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);

    const auto uavDesc = D3D12_UNORDERED_ACCESS_VIEW_DESC{
        .Format = DXGI_FORMAT_R32_TYPELESS,
        .ViewDimension = D3D12_UAV_DIMENSION_BUFFER,
        .Buffer =
            D3D12_BUFFER_UAV{
                .NumElements = static_cast<UINT>(mSize / sizeof(UINT)),
                .Flags = D3D12_BUFFER_UAV_FLAG_RAW,
            },
    };
    context.device.CreateUnorderedAccessView(mResource.Get(), nullptr, &uavDesc,
        descriptor);
    descriptor.Offset(1, context.descriptorSize);
}

D3D12_GPU_VIRTUAL_ADDRESS D3DBuffer::getDeviceAddress()
{
    return mResource->GetGPUVirtualAddress();
}
