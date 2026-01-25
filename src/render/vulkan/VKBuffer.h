#pragma once

#include "VKContext.h"
#include "render/BufferBase.h"

class VKBuffer : public BufferBase
{
public:
    VKBuffer(const Buffer &buffer, VKRenderSession &renderSession);
    explicit VKBuffer(int size);

    const KDGpu::Buffer &buffer() const { return mBuffer; }

    void addUsage(KDGpu::BufferUsageFlags usage);
    void reload();
    void clear(VKContext &context);
    void copy(VKContext &context, VKBuffer &source);
    bool swap(VKBuffer &other);
    void upload(VKContext &context);
    void beginDownload(VKContext &context, bool checkModification);
    bool finishDownload();
    void prepareIndirectBuffer(VKContext &context);
    void prepareVertexBuffer(VKContext &context);
    void prepareIndexBuffer(VKContext &context);
    void prepareUniformBuffer(VKContext &context);
    void prepareShaderStorageBuffer(VKContext &context, bool readable,
        bool writeable);
    void prepareAccelerationStructureGeometry(VKContext &context);
    uint64_t getDeviceAddress(VKContext &context);

private:
    KDGpu::BufferUsageFlags defaultUsage() const;
    void createBuffer(KDGpu::Device &device);
    KDGpu::Buffer createStagingBuffer(KDGpu::Device &device,
        KDGpu::BufferUsageFlagBits usage);
    void updateReadOnlyBuffer(VKContext &context);
    void updateReadWriteBuffer(VKContext &context);
    void memoryBarrier(KDGpu::CommandRecorder &commandRecorder,
        KDGpu::AccessFlags accessMask, KDGpu::PipelineStageFlags stage);

    KDGpu::BufferUsageFlags mUsage{};
    KDGpu::Buffer mBuffer;
    KDGpu::Buffer mDownloadBuffer;
    KDGpu::AccessFlags mCurrentAccessMask{};
    KDGpu::PipelineStageFlags mCurrentStage{};
    bool mDeviceAddressObtained{};
    bool mCheckModification{};
};
