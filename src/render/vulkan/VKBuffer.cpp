#include "VKBuffer.h"
#include "EvaluatedPropertyCache.h"
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

VKBuffer::VKBuffer(const Buffer &buffer, ScriptEngine &scriptEngine)
    : mItemId(buffer.id)
    , mFileName(buffer.fileName)
    , mSize(getBufferSize(buffer, scriptEngine, mMessages))
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
    context.commandRecorder->clearBuffer({
        .dstBuffer = getReadWriteBuffer(context),
        .byteSize = static_cast<uint64_t>(mSize),
    });
}

void VKBuffer::copy(VKContext &context, VKBuffer &source)
{
    context.commandRecorder->copyBuffer({
        .src = source.getReadOnlyBuffer(context),
        .dst = getReadWriteBuffer(context),
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
        .usage = KDGpu::BufferUsageFlagBits::UniformBufferBit
            | KDGpu::BufferUsageFlagBits::StorageBufferBit
            | KDGpu::BufferUsageFlagBits::VertexBufferBit
            | KDGpu::BufferUsageFlagBits::IndexBufferBit
            | KDGpu::BufferUsageFlagBits::IndirectBufferBit
            | KDGpu::BufferUsageFlagBits::TransferSrcBit
            | KDGpu::BufferUsageFlagBits::TransferDstBit,
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

    auto modified = false;
    if (!downloadBuffer(context, getReadOnlyBuffer(context), mSize,
            [&](const void *data) {
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
