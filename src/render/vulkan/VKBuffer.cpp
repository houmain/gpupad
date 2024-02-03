#include "VKBuffer.h"
#include "Singletons.h"
#include "EvaluatedPropertyCache.h"

VKBuffer::VKBuffer(const Buffer &buffer, ScriptEngine &scriptEngine)
    : mItemId(buffer.id)
    , mFileName(buffer.fileName)
    , mSize(getBufferSize(buffer, scriptEngine, mMessages))
{
    mUsedItems += buffer.id;
    for (const auto item : buffer.items)
        if (auto block = static_cast<const Block*>(item)) {
            mUsedItems += block->id;
            for (const auto item : block->items)
                if (auto field = static_cast<const Block*>(item))
                    mUsedItems += field->id;
        }
}

void VKBuffer::updateUntitledFilename(const VKBuffer &rhs)
{
    if (mSize == rhs.mSize &&
        FileDialog::isEmptyOrUntitled(mFileName) &&
        FileDialog::isEmptyOrUntitled(rhs.mFileName))
        mFileName = rhs.mFileName;
}

bool VKBuffer::operator==(const VKBuffer &rhs) const
{
    return std::tie(mFileName, mSize, mMessages) ==
           std::tie(rhs.mFileName, rhs.mSize,rhs.mMessages);
}

void VKBuffer::clear()
{
    // TODO:
}

void VKBuffer::copy(VKBuffer &source)
{
    // TODO:
}

bool VKBuffer::swap(VKBuffer &other)
{
    if (mSize != other.mSize)
        return false;
    mData.swap(other.mData);
    //std::swap(mBufferObject, other.mBufferObject);
    std::swap(mSystemCopyModified, other.mSystemCopyModified);
    std::swap(mDeviceCopyModified, other.mDeviceCopyModified);
    return true;
}

const KDGpu::Buffer &VKBuffer::getReadOnlyBuffer(VKContext &context)
{
    reload();
    createBuffer(context.device);
    upload(context);
    return mBuffer;
}

void VKBuffer::reload()
{
    auto prevData = mData;
    if (!mFileName.isEmpty())
        if (!Singletons::fileCache().getBinary(mFileName, &mData))
            if (!FileDialog::isEmptyOrUntitled(mFileName))
                mMessages += MessageList::insert(
                    mItemId, MessageType::LoadingFileFailed, mFileName);

    if (mSize > mData.size())
        mData.append(QByteArray(mSize - mData.size(), 0));

    mSystemCopyModified |= !mData.isSharedWith(prevData);
}

void VKBuffer::createBuffer(KDGpu::Device &device)
{
    if (mBuffer.isValid())
        return;

    using namespace KDGpu;
    auto bufferOptions = BufferOptions{
        .size = static_cast<DeviceSize>(mSize),
        .usage = BufferUsageFlagBits::UniformBufferBit |
                 BufferUsageFlagBits::StorageBufferBit |
                 BufferUsageFlagBits::VertexBufferBit |
                 BufferUsageFlagBits::IndexBufferBit |
                 BufferUsageFlagBits::IndirectBufferBit |
                 BufferUsageFlagBits::TransferDstBit,
        .memoryUsage = MemoryUsage::GpuOnly
    };
    mBuffer = device.createBuffer(bufferOptions);
}

void VKBuffer::upload(VKContext &context)
{
    if (!mSystemCopyModified)
        return;

    using namespace KDGpu;
    const auto uploadOptions = BufferUploadOptions {
        .destinationBuffer = mBuffer,
        .dstStages = PipelineStageFlagBit::VertexAttributeInputBit,
        .dstMask = AccessFlagBit::VertexAttributeReadBit,
        .data = mData.constData(),
        .byteSize = static_cast<DeviceSize>(mSize)
    };
    context.stagingBuffers.emplace_back(context.queue.uploadBufferData(uploadOptions));

    mSystemCopyModified = mDeviceCopyModified = false;
}

bool VKBuffer::download(bool checkModification)
{
    // TODO:
    return false;
}
