
#include "D3DPrintf.h"

D3DBuffer &D3DPrintf::getInitializedBuffer(D3DContext &context)
{
    if (!mBuffer.has_value())
        mBuffer.emplace(bufferSize);

    const auto header = initializeHeader();
    std::memcpy(mBuffer->writableData().data(), &header, sizeof(BufferHeader));
    return *mBuffer;
}

void D3DPrintf::beginDownload(D3DContext &context)
{
    if (mBuffer)
        mBuffer->beginDownload(context, false);
}

MessagePtrSet D3DPrintf::finishDownload(ItemId callItemId)
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
