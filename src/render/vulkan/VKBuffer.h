#pragma once

#include "VKItem.h"

class VKBuffer
{
public:
    VKBuffer(const Buffer &buffer, VKRenderSession &renderSession);
    void updateUntitledFilename(const VKBuffer &rhs);
    bool operator==(const VKBuffer &rhs) const;

    ItemId itemId() const { return mItemId; }
    const QByteArray &data() const { return mData; }
    const QString &fileName() const { return mFileName; }
    const QSet<ItemId> &usedItems() const { return mUsedItems; }
    int size() const { return mSize; }
    const KDGpu::Buffer &buffer() const { return mBuffer; }

    void addUsage(KDGpu::BufferUsageFlags usage);
    void reload();
    void clear(VKContext &context);
    void copy(VKContext &context, VKBuffer &source);
    bool swap(VKBuffer &other);
    bool download(VKContext &context, bool checkModification);
    void prepareIndirectBuffer(VKContext &context);
    void prepareVertexBuffer(VKContext &context);
    void prepareIndexBuffer(VKContext &context);
    void prepareUniformBuffer(VKContext &context);
    void prepareShaderStorageBuffer(VKContext &context, bool readable,
        bool writeable);
    void prepareAccelerationStructureGeometry(VKContext &context);
    uint64_t getDeviceAddress(VKContext &context);

private:
    void createBuffer(KDGpu::Device &device);
    void upload(VKContext &context);
    void updateReadOnlyBuffer(VKContext &context);
    void updateReadWriteBuffer(VKContext &context);
    void memoryBarrier(KDGpu::CommandRecorder &commandRecorder,
        KDGpu::AccessFlags accessMask, KDGpu::PipelineStageFlags stage);

    MessagePtrSet mMessages;
    ItemId mItemId{};
    QString mFileName;
    int mSize{};
    QByteArray mData;
    QSet<ItemId> mUsedItems;
    KDGpu::BufferUsageFlags mUsage{};
    KDGpu::Buffer mBuffer;
    bool mSystemCopyModified{};
    bool mDeviceCopyModified{};
    KDGpu::AccessFlags mCurrentAccessMask{};
    KDGpu::PipelineStageFlags mCurrentStage{};
    bool mDeviceAddressObtained{};
};

bool downloadBuffer(VKContext &context, const KDGpu::Buffer &buffer,
    uint64_t size, std::function<void(const std::byte *)> &&callback);
