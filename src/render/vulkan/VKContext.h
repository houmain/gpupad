#pragma once

#include <vector>

// still missing declaration in KDGpu/texture_view.h?
namespace KDGpu { class Texture; }
struct ktxVulkanDeviceInfo;

#include <KDGpu/bind_group.h>
#include <KDGpu/device.h>
#include <KDGpu/instance.h>
#include <KDGpu/queue.h>
#include <KDGpu/sampler.h>
#include <KDGpu/texture.h>
#include <KDGpu/texture_view.h>
#include <KDGpu/vulkan/vulkan_graphics_api.h>
#include <KDGpu/bind_group_options.h>
#include <KDGpu/bind_group_layout_options.h>
#include <KDGpu/buffer_options.h>
#include <KDGpu/graphics_pipeline_options.h>
#include <KDGpu/texture_options.h>
#include <KDGpu/compute_pipeline_options.h>

struct VKContext 
{
    KDGpu::Device& device;
    KDGpu::Queue& queue;
    ktxVulkanDeviceInfo& ktxDeviceInfo;
    std::vector<KDGpu::CommandBuffer> commandBuffers;
    std::optional<KDGpu::CommandRecorder> commandRecorder;
    std::map<ItemId, KDGpu::TimestampQueryRecorder> timestampQueries;
};
