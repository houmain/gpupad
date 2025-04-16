#include "VKBuffer.h"
#include "scripting/ScriptEngine.h"
#include "Singletons.h"
#include <QScopeGuard>

namespace {
    class TransferBuffer
    {
    private:
        KDGpu::Buffer mBuffer;

    public:
        TransferBuffer(KDGpu::Device &device, uint64_t size)
        {
            mBuffer = device.createBuffer({
                .size = size,
                .usage = KDGpu::BufferUsageFlagBits::TransferDstBit
                    | KDGpu::BufferUsageFlagBits::TransferSrcBit,
                .memoryUsage = KDGpu::MemoryUsage::CpuOnly,
            });
        }

        bool download(VKContext &context, const KDGpu::Buffer &buffer,
            uint64_t size)
        {
            if (!buffer.isValid() || !mBuffer.isValid())
                return false;

            context.commandRecorder->copyBuffer({
                .src = buffer,
                .dst = mBuffer,
                .byteSize = size,
            });
            return true;
        }

        std::byte *map() { return static_cast<std::byte *>(mBuffer.map()); }

        void unmap() { mBuffer.unmap(); }
    };
} // namespace

bool downloadBuffer(VKContext &context, const KDGpu::Buffer &buffer,
    uint64_t size, std::function<void(const std::byte *)> &&callback)
{
    auto transferBuffer = TransferBuffer(context.device, size);

    context.commandRecorder = context.device.createCommandRecorder();
    auto guard = qScopeGuard([&] { context.commandRecorder.reset(); });

    if (!transferBuffer.download(context, buffer, size))
        return false;

    auto commandBuffer = context.commandRecorder->finish();
    context.queue.submit({ .commandBuffers = { commandBuffer } });
    context.queue.waitUntilIdle();

    auto data = transferBuffer.map();
    callback(data);
    transferBuffer.unmap();
    return true;
}

VKBuffer::VKBuffer(const Buffer &buffer, VKRenderSession &renderSession)
    : mItemId(buffer.id)
    , mFileName(buffer.fileName)
    , mSize(renderSession.getBufferSize(buffer))
{
    // TODO: reduce default usage
    mUsage = KDGpu::BufferUsageFlags{ KDGpu::BufferUsageFlagBits::TransferSrcBit
        | KDGpu::BufferUsageFlagBits::TransferDstBit
        | KDGpu::BufferUsageFlagBits::UniformBufferBit
        | KDGpu::BufferUsageFlagBits::StorageBufferBit
        | KDGpu::BufferUsageFlagBits::VertexBufferBit
        | KDGpu::BufferUsageFlagBits::IndexBufferBit
        | KDGpu::BufferUsageFlagBits::IndirectBufferBit };

    mUsedItems += buffer.id;
    for (const auto item : buffer.items)
        if (auto block = static_cast<const Block *>(item)) {
            mUsedItems += block->id;
            for (const auto item : block->items)
                if (auto field = static_cast<const Block *>(item))
                    mUsedItems += field->id;
        }
}

void VKBuffer::addUsage(KDGpu::BufferUsageFlags usage)
{
    // TODO: not ideal to update usage of already created buffer
    if ((mUsage & usage) != usage)
        mBuffer = {};
    mUsage |= usage;
}

void VKBuffer::updateUntitledFilename(const VKBuffer &rhs)
{
    if (mSize == rhs.mSize && FileDialog::isEmptyOrUntitled(mFileName)
        && FileDialog::isEmptyOrUntitled(rhs.mFileName))
        mFileName = rhs.mFileName;
}

bool VKBuffer::operator==(const VKBuffer &rhs) const
{
    return std::tie(mFileName, mSize, mMessages)
        == std::tie(rhs.mFileName, rhs.mSize, rhs.mMessages);
}

void VKBuffer::clear(VKContext &context)
{
    updateReadWriteBuffer(context);

    memoryBarrier(*context.commandRecorder,
        KDGpu::AccessFlagBit::MemoryWriteBit,
        KDGpu::PipelineStageFlagBit::AllCommandsBit);

    context.commandRecorder->clearBuffer({
        .dstBuffer = buffer(),
        .byteSize = static_cast<uint64_t>(mSize),
    });
}

void VKBuffer::copy(VKContext &context, VKBuffer &source)
{
    source.updateReadOnlyBuffer(context);

    source.memoryBarrier(*context.commandRecorder,
        KDGpu::AccessFlagBit::MemoryReadBit,
        KDGpu::PipelineStageFlagBit::AllCommandsBit);

    updateReadWriteBuffer(context);

    memoryBarrier(*context.commandRecorder,
        KDGpu::AccessFlagBit::MemoryWriteBit,
        KDGpu::PipelineStageFlagBit::AllCommandsBit);

    context.commandRecorder->copyBuffer({
        .src = source.buffer(),
        .dst = buffer(),
        .byteSize = static_cast<uint64_t>(mSize),
    });
}

bool VKBuffer::swap(VKBuffer &other)
{
    if (mSize != other.mSize)
        return false;
    mData.swap(other.mData);
    std::swap(mBuffer, other.mBuffer);
    std::swap(mSystemCopyModified, other.mSystemCopyModified);
    std::swap(mDeviceCopyModified, other.mDeviceCopyModified);
    return true;
}

void VKBuffer::updateReadOnlyBuffer(VKContext &context)
{
    reload();
    createBuffer(context.device);
    upload(context);
}

void VKBuffer::updateReadWriteBuffer(VKContext &context)
{
    reload();
    createBuffer(context.device);
    upload(context);
    mDeviceCopyModified = true;
}

void VKBuffer::reload()
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

void VKBuffer::createBuffer(KDGpu::Device &device)
{
    if (mBuffer.isValid())
        return;

    mBuffer = device.createBuffer({
        .size = static_cast<KDGpu::DeviceSize>(mSize),
        .usage = mUsage,
        .memoryUsage = KDGpu::MemoryUsage::GpuOnly,
    });
}

void VKBuffer::upload(VKContext &context)
{
    if (!mSystemCopyModified)
        return;

    context.queue.waitForUploadBufferData({
        .destinationBuffer = mBuffer,
        .data = mData.constData(),
        .byteSize = static_cast<KDGpu::DeviceSize>(mSize),
    });
    mSystemCopyModified = mDeviceCopyModified = false;
}

bool VKBuffer::download(VKContext &context, bool checkModification)
{
    if (!mDeviceCopyModified)
        return false;

    updateReadOnlyBuffer(context);

    auto modified = false;
    if (!downloadBuffer(context, buffer(), mSize, [&](const void *data) {
            if (!checkModification
                || std::memcmp(mData.data(), data, mData.size()) != 0) {
                std::memcpy(mData.data(), data, mData.size());
                modified = true;
            }
        })) {
        mMessages +=
            MessageList::insert(mItemId, MessageType::DownloadingImageFailed);
        return false;
    }

    mSystemCopyModified = mDeviceCopyModified = false;
    return modified;
}

void VKBuffer::memoryBarrier(KDGpu::CommandRecorder &commandRecorder,
    KDGpu::AccessFlags accessMask, KDGpu::PipelineStageFlags stage)
{
    if (!mBuffer.isValid())
        return;

    commandRecorder.bufferMemoryBarrier(KDGpu::BufferMemoryBarrierOptions{
        .srcStages = mCurrentStage,
        .srcMask = mCurrentAccessMask,
        .dstStages = stage,
        .dstMask = accessMask,
        .buffer = mBuffer,
    });
    mCurrentStage = stage;
    mCurrentAccessMask = accessMask;
}

void VKBuffer::prepareIndirectBuffer(VKContext &context)
{
    updateReadOnlyBuffer(context);

    memoryBarrier(*context.commandRecorder, KDGpu::AccessFlagBit::IndexReadBit,
        KDGpu::PipelineStageFlagBit::AllCommandsBit);
}

void VKBuffer::prepareVertexBuffer(VKContext &context)
{
    updateReadOnlyBuffer(context);

    memoryBarrier(*context.commandRecorder,
        KDGpu::AccessFlagBit::VertexAttributeReadBit,
        KDGpu::PipelineStageFlagBit::AllCommandsBit);
}

void VKBuffer::prepareIndexBuffer(VKContext &context)
{
    updateReadOnlyBuffer(context);

    memoryBarrier(*context.commandRecorder, KDGpu::AccessFlagBit::IndexReadBit,
        KDGpu::PipelineStageFlagBit::AllCommandsBit);
}

void VKBuffer::prepareUniformBuffer(VKContext &context)
{
    updateReadOnlyBuffer(context);

    memoryBarrier(*context.commandRecorder,
        KDGpu::AccessFlagBit::UniformReadBit,
        KDGpu::PipelineStageFlagBit::AllCommandsBit);
}

void VKBuffer::prepareShaderStorageBuffer(VKContext &context)
{
    updateReadWriteBuffer(context);

    memoryBarrier(*context.commandRecorder,
        KDGpu::AccessFlagBit::ShaderStorageReadBit
            | KDGpu::AccessFlagBit::ShaderStorageWriteBit,
        KDGpu::PipelineStageFlagBit::AllCommandsBit);
}

void VKBuffer::prepareAccelerationStructureGeometry(VKContext &context)
{
    updateReadOnlyBuffer(context);

    memoryBarrier(*context.commandRecorder,
        KDGpu::AccessFlagBit::AccelerationStructureReadBit,
        KDGpu::PipelineStageFlagBit::AllCommandsBit);
}
