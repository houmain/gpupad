#pragma once

#include <vector>

// still missing declaration in KDGpu/texture_view.h?
namespace KDGpu {
    class Texture;
}
struct ktxVulkanDeviceInfo;

#include "session/Item.h"
#include <KDGpu/acceleration_structure.h>
#include <KDGpu/bind_group.h>
#include <KDGpu/bind_group_layout_options.h>
#include <KDGpu/bind_group_options.h>
#include <KDGpu/buffer_options.h>
#include <KDGpu/compute_pipeline_options.h>
#include <KDGpu/device.h>
#include <KDGpu/graphics_pipeline_options.h>
#include <KDGpu/instance.h>
#include <KDGpu/queue.h>
#include <KDGpu/raytracing_shader_binding_table.h>
#include <KDGpu/raytracing_pipeline_options.h>
#include <KDGpu/sampler.h>
#include <KDGpu/texture.h>
#include <KDGpu/texture_options.h>
#include <KDGpu/texture_view.h>
#include <KDGpu/vulkan/vulkan_graphics_api.h>

struct VKContext
{
    KDGpu::Device &device;
    KDGpu::Queue &queue;
    ktxVulkanDeviceInfo &ktxDeviceInfo;
    std::vector<KDGpu::CommandBuffer> commandBuffers;
    std::optional<KDGpu::CommandRecorder> commandRecorder;
    std::map<ItemId, KDGpu::TimestampQueryRecorder> timestampQueries;

    const KDGpu::AdapterFeatures &features() const
    {
        return device.adapter()->features();
    }

    const KDGpu::AdapterLimits &adapterLimits() const
    {
        return device.adapter()->properties().limits;
    }
};
