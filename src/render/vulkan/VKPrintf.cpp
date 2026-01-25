
#include "VKPrintf.h"

VKBuffer &VKPrintf::getInitializedBuffer(VKContext &context)
{
    if (!mBuffer.has_value())
        mBuffer.emplace(bufferSize);

    const auto header = initializeHeader();
    std::memcpy(mBuffer->writableData().data(), &header, sizeof(BufferHeader));
    return *mBuffer;
}

void VKPrintf::beginDownload(VKContext &context)
{
    if (mBuffer)
        mBuffer->beginDownload(context, false);
}

MessagePtrSet VKPrintf::finishDownload(ItemId callItemId)
{
    if (!mBuffer || !mBuffer->finishDownload())
        return {};
    const auto data = mBuffer->data().constData();
    const auto &header = *reinterpret_cast<const BufferHeader *>(data);
    const auto count =
        (header.offset > maxBufferValues ? maxBufferValues : header.offset);
    return PrintfBase::formatMessages(callItemId, header,
        { reinterpret_cast<const uint32_t *>(data + sizeof(BufferHeader)),
            count });
}
