#include "VKBuffer.h"
#include "Singletons.h"
#include "EvaluatedPropertyCache.h"

namespace
{
    class TransferBuffer
    {
    private:
        KDGpu::Buffer mBuffer;

    public:
        TransferBuffer(KDGpu::Device &device, uint64_t size)
        {
            mBuffer = device.createBuffer({
                .size = size,
                .usage = KDGpu::BufferUsageFlagBits::TransferDstBit | 
                         KDGpu::BufferUsageFlagBits::TransferSrcBit,
                .memoryUsage = KDGpu::MemoryUsage::CpuOnly
            });
        }

        bool download(VKContext &context, VKBuffer &buffer)
        {
            if (!mBuffer.isValid())
                return false;

            context.commandRecorder->copyBuffer({
                .src = buffer.getReadOnlyBuffer(context),
                .dst = mBuffer,
                .byteSize = static_cast<uint64_t>(buffer.size()),
            });
            return true;
        }

        std::byte *map()
        {
            return static_cast<std::byte*>(mBuffer.map());
        }

        void unmap()
        {
            mBuffer.unmap();
        }
    };
} // namespace

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
    std::swap(mBuffer, other.mBuffer);
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

const KDGpu::Buffer &VKBuffer::getReadWriteBuffer(VKContext &context)
{
    reload();
    createBuffer(context.device);
    upload(context);
    mDeviceCopyModified = true;
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

    mBuffer = device.createBuffer({
        .size = static_cast<KDGpu::DeviceSize>(mSize),
        .usage = KDGpu::BufferUsageFlagBits::UniformBufferBit |
                 KDGpu::BufferUsageFlagBits::StorageBufferBit |
                 KDGpu::BufferUsageFlagBits::VertexBufferBit |
                 KDGpu::BufferUsageFlagBits::IndexBufferBit |
                 KDGpu::BufferUsageFlagBits::IndirectBufferBit |
                 KDGpu::BufferUsageFlagBits::TransferSrcBit |
                 KDGpu::BufferUsageFlagBits::TransferDstBit,
        .memoryUsage = KDGpu::MemoryUsage::GpuOnly
    });
}

void VKBuffer::upload(VKContext &context)
{
    if (!mSystemCopyModified)
        return;

    context.queue.waitForUploadBufferData({
        .destinationBuffer = mBuffer,
        .data = mData.constData(),
        .byteSize = static_cast<KDGpu::DeviceSize>(mSize)
    });
    mSystemCopyModified = mDeviceCopyModified = false;
}

bool VKBuffer::download(VKContext &context, bool checkModification)
{
    if (!mDeviceCopyModified)
        return false;

    auto prevData = QByteArray();
    if (checkModification)
        prevData = mData;

    auto transferBuffer = TransferBuffer(context.device, 
        static_cast<uint64_t>(mSize));

    context.commandRecorder = context.device.createCommandRecorder();
    auto guard = qScopeGuard([&] { context.commandRecorder.reset(); });

    if (!transferBuffer.download(context, *this)) {
        mMessages += MessageList::insert(
            mItemId, MessageType::DownloadingImageFailed);
        return false;
    }

    auto commandBuffer = context.commandRecorder->finish();
    context.queue.submit({ .commandBuffers = { commandBuffer} });
    context.queue.waitUntilIdle();

    auto data = transferBuffer.map();
    std::memcpy(mData.data(), data, mData.size());
    transferBuffer.unmap();

    mSystemCopyModified = mDeviceCopyModified = false;

    if (checkModification && prevData == mData) {
        mData = prevData;
        return false;
    }
    return true;
}
