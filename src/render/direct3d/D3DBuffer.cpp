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

            if (!mElementSize)
                mElementSize = getBlockStride(*block);
        }
}

D3DBuffer::D3DBuffer(int size) : BufferBase(size) { }

void D3DBuffer::clear(D3DContext &context)
{
    // see https://asawicki.info/news_1795_secrets_of_direct3d_12_the_behavior_of_clearunorderedaccessviewuintfloat
    if (!mShaderVisibleDescHeap) {
        auto descriptorHeapDesc = D3D12_DESCRIPTOR_HEAP_DESC{
            .Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV,
            .NumDescriptors = 1,
            .Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE,
            .NodeMask = 0,
        };
        AssertIfFailed(context.device.CreateDescriptorHeap(&descriptorHeapDesc,
            IID_PPV_ARGS(&mShaderVisibleDescHeap)));

        descriptorHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
        AssertIfFailed(context.device.CreateDescriptorHeap(&descriptorHeapDesc,
            IID_PPV_ARGS(&mNonShaderVisibleDescHeap)));
    }

    auto shaderVisibleGpuDescHandle = CD3DX12_GPU_DESCRIPTOR_HANDLE(
        mShaderVisibleDescHeap->GetGPUDescriptorHandleForHeapStart());
    auto shaderVisibleCpuDescHandle = CD3DX12_CPU_DESCRIPTOR_HANDLE(
        mShaderVisibleDescHeap->GetCPUDescriptorHandleForHeapStart());
    auto nonShaderVisibleCpuDescHandle = CD3DX12_CPU_DESCRIPTOR_HANDLE(
        mNonShaderVisibleDescHeap->GetCPUDescriptorHandleForHeapStart());

    prepareUnorderedAccessView(context, shaderVisibleCpuDescHandle, false,
        false);
    prepareUnorderedAccessView(context, nonShaderVisibleCpuDescHandle, false,
        false);

    auto descriptorHeaps = mShaderVisibleDescHeap.Get();
    context.graphicsCommandList->SetDescriptorHeaps(1, &descriptorHeaps);

    UINT values[4] = { 0, 0, 0, 0 };
    context.graphicsCommandList->ClearUnorderedAccessViewUint(
        shaderVisibleGpuDescHandle, nonShaderVisibleCpuDescHandle,
        mResource.Get(), values, 0, nullptr);
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

void D3DBuffer::upload(D3DContext &context)
{
    if (!mSystemCopyModified)
        return;

    createBuffer(context);

    auto stagingBuffer = createStagingBuffer(context, D3D12_HEAP_TYPE_UPLOAD);
    auto mappedData = std::add_pointer_t<void>{};
    AssertIfFailed(stagingBuffer->Map(0, nullptr, &mappedData));
    std::memcpy(mappedData, mData.constData(), mSize);
    stagingBuffer->Unmap(0, nullptr);

    resourceBarrier(context, D3D12_RESOURCE_STATE_COPY_DEST);
    context.graphicsCommandList->CopyBufferRegion(mResource.Get(), 0,
        stagingBuffer.Get(), 0, static_cast<UINT64>(mSize));

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
        Q_ASSERT(mData.size() >= mSize);
        if (!mCheckModification
            || std::memcmp(mData.data(), mappedData, mSize) != 0) {
            std::memcpy(mData.data(), mappedData, mSize);
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

void D3DBuffer::prepareCopySource(D3DContext &context)
{
    updateReadOnlyBuffer(context);
    resourceBarrier(context, D3D12_RESOURCE_STATE_COPY_SOURCE);
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
    D3D12_CPU_DESCRIPTOR_HANDLE descriptor)
{
    updateReadOnlyBuffer(context);
    resourceBarrier(context, D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER);

    const auto cbvDesc = D3D12_CONSTANT_BUFFER_VIEW_DESC{
        .BufferLocation = getDeviceAddress(),
        .SizeInBytes = alignedSize(),
    };
    context.device.CreateConstantBufferView(&cbvDesc, descriptor);
}

void D3DBuffer::prepareUnorderedAccessView(D3DContext &context,
    D3D12_CPU_DESCRIPTOR_HANDLE descriptor, bool isStructured, bool isReadonly)
{
    if (isReadonly) {
        updateReadOnlyBuffer(context);
    } else {
        updateReadWriteBuffer(context);
    }
    resourceBarrier(context, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);

    auto uavDesc = D3D12_UNORDERED_ACCESS_VIEW_DESC{
        .ViewDimension = D3D12_UAV_DIMENSION_BUFFER,
    };
    if (isStructured) {
        uavDesc.Buffer = D3D12_BUFFER_UAV{
            .NumElements = static_cast<UINT>(mSize / mElementSize),
            .StructureByteStride = mElementSize,
            .Flags = D3D12_BUFFER_UAV_FLAG_NONE,
        };
    } else {
        uavDesc.Format = DXGI_FORMAT_R32_TYPELESS;
        uavDesc.Buffer = D3D12_BUFFER_UAV{
            .NumElements = static_cast<UINT>(mSize / sizeof(UINT)),
            .StructureByteStride = 0,
            .Flags = D3D12_BUFFER_UAV_FLAG_RAW,
        };
    }
    context.device.CreateUnorderedAccessView(mResource.Get(), nullptr, &uavDesc,
        descriptor);
}

void D3DBuffer::prepareShaderResourceView(D3DContext &context,
    D3D12_CPU_DESCRIPTOR_HANDLE descriptor, bool isStructured)
{
    updateReadOnlyBuffer(context);
    resourceBarrier(context, D3D12_RESOURCE_STATE_ALL_SHADER_RESOURCE);

    auto srvDesc = D3D12_SHADER_RESOURCE_VIEW_DESC{
        .ViewDimension = D3D12_SRV_DIMENSION_BUFFER,
        .Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING,
    };
    if (isStructured) {
        srvDesc.Buffer = D3D12_BUFFER_SRV{
            .NumElements = static_cast<UINT>(mSize / mElementSize),
            .StructureByteStride = mElementSize,
            .Flags = D3D12_BUFFER_SRV_FLAG_NONE,
        };
    } else {
        srvDesc.Format = DXGI_FORMAT_R32_TYPELESS;
        srvDesc.Buffer = D3D12_BUFFER_SRV{
            .NumElements = static_cast<UINT>(mSize / sizeof(UINT)),
            .StructureByteStride = 0,
            .Flags = D3D12_BUFFER_SRV_FLAG_RAW,
        };
    }
    context.device.CreateShaderResourceView(mResource.Get(), &srvDesc,
        descriptor);
}

D3D12_GPU_VIRTUAL_ADDRESS D3DBuffer::getDeviceAddress()
{
    return mResource->GetGPUVirtualAddress();
}
