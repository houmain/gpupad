
#include "VKPrintf.h"
#include "VKBuffer.h"

KDGpu::Buffer &VKPrintf::getInitializedBuffer(VKContext &context)
{
    if (!mBuffer.isValid()) {
        mBuffer = context.device.createBuffer({
            .size = static_cast<KDGpu::DeviceSize>(
                maxBufferValues * sizeof(uint32_t) + sizeof(BufferHeader)),
            .usage = KDGpu::BufferUsageFlagBits::StorageBufferBit |
                     KDGpu::BufferUsageFlagBits::TransferSrcBit |
                     KDGpu::BufferUsageFlagBits::TransferDstBit,
            .memoryUsage = KDGpu::MemoryUsage::GpuOnly
        });
    }

    auto header = initializeHeader();
    context.queue.waitForUploadBufferData({
        .destinationBuffer = mBuffer,
        .data = &header,
        .byteSize = sizeof(BufferHeader)
    });
    return mBuffer;
}

MessagePtrSet VKPrintf::formatMessages(VKContext &context, ItemId callItemId)
{
    if (!mBuffer.isValid())
        return { };

    auto messages = MessagePtrSet{ };
    downloadBuffer(context, mBuffer,
        uint64_t{ maxBufferValues * sizeof(uint32_t) + sizeof(BufferHeader) },
        [&](const std::byte *data) {
            const auto &header = *reinterpret_cast<const BufferHeader*>(data);
            const auto count = (header.offset > maxBufferValues ? 
                maxBufferValues : header.offset);
            if (count > 1)
                messages = ShaderPrintf::formatMessages(callItemId, header, 
                  { reinterpret_cast<const uint32_t*>(data + sizeof(BufferHeader)), count });
        });

    return messages;
}
