#pragma once

#include "VKItem.h"
#include "scripting/ScriptEngine.h"

class VKBuffer
{
public:
    VKBuffer(const Buffer &buffer, ScriptEngine &scriptEngine);
    void updateUntitledFilename(const VKBuffer &rhs);
    bool operator==(const VKBuffer &rhs) const;

    ItemId itemId() const { return mItemId; }
    const QByteArray &data() const { return mData; }
    const QString &fileName() const { return mFileName; }
    const QSet<ItemId> &usedItems() const { return mUsedItems; }
    int size() const { return mSize; }
    const KDGpu::Buffer &buffer() const { return mBuffer; }

    void addUsage(KDGpu::BufferUsageFlags usage);
    void clear(VKContext &context);
    void copy(VKContext &context, VKBuffer &source);
    bool swap(VKBuffer &other);
    bool download(VKContext &context, bool checkModification);
    void prepareIndirectBuffer(VKContext &context);
    void prepareVertexBuffer(VKContext &context);
    void prepareIndexBuffer(VKContext &context);
    void prepareUniformBuffer(VKContext &context);
    void prepareShaderStorageBuffer(VKContext &context);
    void prepareAccelerationStructureGeometry(VKContext &context);

private:
    void reload();
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
};

int getBufferSize(const Buffer &buffer, ScriptEngine &scriptEngine,
    MessagePtrSet &messages);
bool downloadBuffer(VKContext &context, const KDGpu::Buffer &buffer,
    uint64_t size, std::function<void(const std::byte *)> &&callback);
