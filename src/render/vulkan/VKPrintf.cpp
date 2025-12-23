
#include "VKPrintf.h"
#include "VKBuffer.h"

KDGpu::Buffer &VKPrintf::getInitializedBuffer(VKContext &context)
{
    if (!mBuffer.isValid()) {
        mBuffer = context.device.createBuffer({
            .size = static_cast<KDGpu::DeviceSize>(bufferSize),
            .usage = KDGpu::BufferUsageFlagBits::StorageBufferBit
                | KDGpu::BufferUsageFlagBits::TransferSrcBit
                | KDGpu::BufferUsageFlagBits::TransferDstBit,
            .memoryUsage = KDGpu::MemoryUsage::GpuOnly,
        });
    }

    auto header = initializeHeader();
    context.queue.waitForUploadBufferData({
        .destinationBuffer = mBuffer,
        .data = &header,
        .byteSize = sizeof(BufferHeader),
    });
    return mBuffer;
}

void VKPrintf::beginDownload(VKContext &context)
{
    if (!mBuffer.isValid())
        return;

    if (!mDownloadBuffer.isValid())
        mDownloadBuffer = context.device.createBuffer({
            .size = static_cast<KDGpu::DeviceSize>(bufferSize),
            .usage = KDGpu::BufferUsageFlagBits::TransferDstBit
                | KDGpu::BufferUsageFlagBits::TransferSrcBit,
            .memoryUsage = KDGpu::MemoryUsage::CpuOnly,
        });

    if (mDownloadBuffer.isValid()) {
        auto commandRecorder = context.device.createCommandRecorder();
        commandRecorder.copyBuffer({
            .src = mBuffer,
            .dst = mDownloadBuffer,
            .byteSize = static_cast<KDGpu::DeviceSize>(bufferSize),
        });
        context.commandBuffers.push_back(commandRecorder.finish());
    }
}

MessagePtrSet VKPrintf::finishDownload(ItemId callItemId)
{
    if (!mDownloadBuffer.isValid())
        return {};

    auto messages = MessagePtrSet{};
    const auto data = static_cast<const uint8_t *>(mDownloadBuffer.map());
    const auto &header = *reinterpret_cast<const BufferHeader *>(data);
    const auto count =
        (header.offset > maxBufferValues ? maxBufferValues : header.offset);
    messages = ShaderPrintf::formatMessages(callItemId, header,
      { reinterpret_cast<const uint32_t *>(data + sizeof(BufferHeader)), count });
    mDownloadBuffer.unmap();
    return messages;
}
