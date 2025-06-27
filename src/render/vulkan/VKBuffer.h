#pragma once

#include "VKItem.h"
#include "render/BufferBase.h"

class VKBuffer : public BufferBase
{
public:
    VKBuffer(const Buffer &buffer, VKRenderSession &renderSession);

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

    KDGpu::BufferUsageFlags mUsage{};
    KDGpu::Buffer mBuffer;
    KDGpu::AccessFlags mCurrentAccessMask{};
    KDGpu::PipelineStageFlags mCurrentStage{};
    bool mDeviceAddressObtained{};
};

bool downloadBuffer(VKContext &context, const KDGpu::Buffer &buffer,
    uint64_t size, std::function<void(const std::byte *)> &&callback);
