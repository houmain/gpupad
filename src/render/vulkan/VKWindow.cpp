#include "VKWindow.h"
#include "VKContext.h"

#include <QCoreApplication>
#include <QDebug>
#include <QEvent>
#include <algorithm>
#include <array>
#include <chrono>
#include <cstdio>
#include <optional>
#include <thread>

// TODO: added because of multiple definitions of fmt::v11::detail::assert_fail
#if defined(_WIN32) && !defined(FMT_ASSERT)
#  define FMT_ASSERT
#endif

#include <KDGpu/adapter.h>
#include <KDGpu/adapter_swapchain_properties.h>
#include <KDGpu/command_buffer.h>
#include <KDGpu/command_recorder.h>
#include <KDGpu/device.h>
#include <KDGpu/fence.h>
#include <KDGpu/gpu_semaphore.h>
#include <KDGpu/instance.h>
#include <KDGpu/queue.h>
#include <KDGpu/render_pass_command_recorder.h>
#include <KDGpu/render_pass_command_recorder_options.h>
#include <KDGpu/surface.h>
#include <KDGpu/surface_options.h>
#include <KDGpu/swapchain.h>
#include <KDGpu/swapchain_options.h>
#include <KDGpu/texture.h>
#include <KDGpu/texture_view.h>
#include <KDGpu/texture_view_options.h>
#include <KDGpu/vulkan/vulkan_adapter.h>
#include <KDGpu/vulkan/vulkan_device.h>
#include <KDGpu/vulkan/vulkan_graphics_api.h>
#include <KDGpu/vulkan/vulkan_instance.h>
#include <KDGpu/vulkan/vulkan_queue.h>
#include <ktxvulkan.h>

namespace {
    constexpr auto MaxFramesInFlight = 2u;

    uint32_t applicationVersion()
    {
        auto major = 0;
        auto minor = 0;
        auto build = 0;
        std::sscanf(qPrintable(QCoreApplication::applicationVersion()),
            "%d.%d.%d", &major, &minor, &build);
        return KDGPU_MAKE_API_VERSION(0, major, minor, build);
    }

    KDGpu::SurfaceOptions surfaceOptions(QWindow &window)
    {
        auto options = KDGpu::SurfaceOptions{};
#if defined(Q_OS_WIN)
        options.hWnd = reinterpret_cast<HWND>(window.winId());
#else
        Q_UNUSED(window);
#endif
        return options;
    }

    KDGpu::PresentMode choosePresentMode(
        const std::vector<KDGpu::PresentMode> &availableModes, bool vsync)
    {
        const auto preferredModes = vsync
            ? std::array{ KDGpu::PresentMode::Fifo,
                  KDGpu::PresentMode::FifoRelaxed, KDGpu::PresentMode::Mailbox,
                  KDGpu::PresentMode::Immediate }
            : std::array{ KDGpu::PresentMode::Immediate,
                  KDGpu::PresentMode::Mailbox, KDGpu::PresentMode::FifoRelaxed,
                  KDGpu::PresentMode::Fifo };

        for (const auto mode : preferredModes)
            if (std::ranges::find(availableModes, mode) != availableModes.end())
                return mode;

        return KDGpu::PresentMode::Fifo;
    }

    KDGpu::Format chooseSwapchainFormat(
        const std::vector<KDGpu::SurfaceFormat> &availableFormats,
        KDGpu::ColorSpace *colorSpace)
    {
        const auto preferredFormats = std::array{
            KDGpu::Format::B8G8R8A8_UNORM,
            KDGpu::Format::R8G8B8A8_UNORM,
            KDGpu::Format::B8G8R8A8_SRGB,
            KDGpu::Format::R8G8B8A8_SRGB,
        };

        for (const auto format : preferredFormats)
            for (const auto &availableFormat : availableFormats)
                if (availableFormat.format == format
                    && availableFormat.colorSpace
                        == KDGpu::ColorSpace::SRgbNonlinear) {
                    *colorSpace = availableFormat.colorSpace;
                    return availableFormat.format;
                }

        *colorSpace = availableFormats.front().colorSpace;
        return availableFormats.front().format;
    }

    KDGpu::CompositeAlphaFlagBits chooseCompositeAlpha(
        KDGpu::CompositeAlphaFlags supportedCompositeAlpha)
    {
        const auto preferredAlpha = std::array{
            KDGpu::CompositeAlphaFlagBits::OpaqueBit,
            KDGpu::CompositeAlphaFlagBits::PreMultipliedBit,
            KDGpu::CompositeAlphaFlagBits::PostMultipliedBit,
            KDGpu::CompositeAlphaFlagBits::InheritBit,
        };

        for (const auto alpha : preferredAlpha)
            if (supportedCompositeAlpha.testFlag(alpha))
                return alpha;

        return KDGpu::CompositeAlphaFlagBits::OpaqueBit;
    }

    class SharedGpuContext final
    {
    public:
        SharedGpuContext()
        {
            api = std::make_unique<KDGpu::VulkanGraphicsApi>();

            auto instanceOptions = KDGpu::InstanceOptions{
                .applicationName =
                    QCoreApplication::applicationName().toStdString(),
                .applicationVersion = applicationVersion(),
#if !defined(NDEBUG)
                .layers = { "VK_LAYER_KHRONOS_validation" },
#endif
            };
            instance = api->createInstance(instanceOptions);
        }

        ~SharedGpuContext()
        {
            if (device.isValid())
                device.waitUntilIdle();
            releaseKtx();
        }

        bool initializeDevice(KDGpu::Surface &surface)
        {
            if (!instance.isValid() || instance.adapters().empty()) {
                qWarning() << "Creating KDGpu Vulkan instance failed";
                return false;
            }

            if (!device.isValid()) {
                auto defaultDevice = instance.createDefaultDevice(surface);
                adapter = defaultDevice.adapter;
                device = std::move(defaultDevice.device);
                if (!adapter || !device.isValid()) {
                    qWarning() << "Creating KDGpu Vulkan device failed";
                    return false;
                }

                const auto requiredQueueFlags =
                    KDGpu::QueueFlags(KDGpu::QueueFlagBits::GraphicsBit
                        | KDGpu::QueueFlagBits::TransferBit);
                for (const auto &candidate : device.queues())
                    if ((candidate.flags() & requiredQueueFlags)
                            == requiredQueueFlags
                        && adapter->supportsPresentation(surface,
                            candidate.queueTypeIndex())) {
                        queue = candidate;
                        break;
                    }
                if (!queue.isValid())
                    for (const auto &candidate : device.queues())
                        if ((candidate.flags()
                                & KDGpu::QueueFlagBits::GraphicsBit)
                            && adapter->supportsPresentation(surface,
                                candidate.queueTypeIndex())) {
                            queue = candidate;
                            break;
                        }
                if (!queue.isValid()) {
                    qWarning()
                        << "No KDGpu Vulkan graphics/present queue found";
                    return false;
                }
            } else if (!adapter->supportsPresentation(surface,
                           queue.queueTypeIndex())) {
                qWarning() << "Shared KDGpu device queue cannot present to "
                              "this window";
                return false;
            }

            return initializeKtx();
        }

        bool initializeKtx()
        {
            if (ktxDeviceInfoInitialized)
                return true;

            const auto &rm = *device.graphicsApi()->resourceManager();
            const auto vkInstance = rm.getInstance(instance);
            const auto vkAdapter = rm.getAdapter(*adapter);
            const auto vkDevice = rm.getDevice(device);
            const auto vkQueue = rm.getQueue(queue);

            const auto commandPoolInfo = VkCommandPoolCreateInfo{
                .sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
                .flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT,
                .queueFamilyIndex = queue.queueTypeIndex(),
            };
            if (vkCreateCommandPool(vkDevice->device, &commandPoolInfo, nullptr,
                    &ktxCommandPool)
                != VK_SUCCESS) {
                qWarning() << "Creating Vulkan command pool for KTX failed";
                return false;
            }

            if (ktxVulkanDeviceInfo_ConstructEx(&ktxDeviceInfo,
                    vkInstance->instance, vkAdapter->physicalDevice,
                    vkDevice->device, vkQueue->queue, ktxCommandPool, nullptr,
                    nullptr)
                != KTX_SUCCESS) {
                qWarning() << "Creating KTX Vulkan device info failed";
                releaseKtx();
                return false;
            }

            ktxDeviceInfoInitialized = true;
            return true;
        }

        void releaseKtx()
        {
            if (ktxDeviceInfoInitialized) {
                ktxVulkanDeviceInfo_Destruct(&ktxDeviceInfo);
                ktxDeviceInfoInitialized = false;
                ktxDeviceInfo = {};
            }

            if (ktxCommandPool && device.isValid()) {
                const auto &rm = *device.graphicsApi()->resourceManager();
                const auto vkDevice =
                    static_cast<KDGpu::VulkanDevice *>(rm.getDevice(device));
                vkDestroyCommandPool(vkDevice->device, ktxCommandPool, nullptr);
                ktxCommandPool = VK_NULL_HANDLE;
            }
        }

        std::unique_ptr<KDGpu::VulkanGraphicsApi> api;
        KDGpu::Instance instance;
        KDGpu::Adapter *adapter{};
        KDGpu::Device device;
        KDGpu::Queue queue;
        ktxVulkanDeviceInfo ktxDeviceInfo{};
        VkCommandPool ktxCommandPool{};
        bool ktxDeviceInfoInitialized{};
    };

    std::weak_ptr<SharedGpuContext> sSharedGpuContext;

    std::shared_ptr<SharedGpuContext> sharedGpuContext()
    {
        auto context = sSharedGpuContext.lock();
        if (!context) {
            context = std::make_shared<SharedGpuContext>();
            sSharedGpuContext = context;
        }
        return context;
    }
} // namespace

struct VKWindow::State
{
    std::shared_ptr<SharedGpuContext> shared;
    KDGpu::Surface surface;
    KDGpu::Swapchain swapchain;
    std::vector<KDGpu::TextureView> swapchainViews;
    std::array<KDGpu::GpuSemaphore, MaxFramesInFlight>
        presentCompleteSemaphores;
    std::vector<KDGpu::GpuSemaphore> renderCompleteSemaphores;
    std::array<KDGpu::Fence, MaxFramesInFlight> frameFences;
    std::array<KDGpu::CommandBuffer, MaxFramesInFlight> commandBuffers;
    KDGpu::Format swapchainFormat{ KDGpu::Format::B8G8R8A8_UNORM };
    KDGpu::ColorSpace colorSpace{ KDGpu::ColorSpace::SRgbNonlinear };
    KDGpu::PresentMode presentMode{ KDGpu::PresentMode::Fifo };
    KDGpu::CompositeAlphaFlagBits compositeAlpha{
        KDGpu::CompositeAlphaFlagBits::OpaqueBit
    };
    KDGpu::Extent2D swapchainExtent{};
    uint32_t currentSwapchainImageIndex{};
    uint32_t inFlightIndex{};
    bool swapchainDirty{ true };
    std::optional<KDGpu::RenderPassCommandRecorder> renderPass;
};

bool VKWindow::isSupported()
{
    auto context = sharedGpuContext();
    return context->instance.isValid() && !context->instance.adapters().empty();
}

VKWindow::VKWindow(int syncInterval) : mSyncInterval(syncInterval)
{
    setSurfaceType(QWindow::VulkanSurface);
}

VKWindow::~VKWindow()
{
    releaseGpu();
}

KDGpu::Device &VKWindow::device()
{
    Q_ASSERT(mState && mState->shared);
    return mState->shared->device;
}

KDGpu::Queue &VKWindow::queue()
{
    Q_ASSERT(mState && mState->shared);
    return mState->shared->queue;
}

ktxVulkanDeviceInfo &VKWindow::ktxDeviceInfo()
{
    Q_ASSERT(mState && mState->shared
        && mState->shared->ktxDeviceInfoInitialized);
    return mState->shared->ktxDeviceInfo;
}

KDGpu::RenderPassCommandRecorder &VKWindow::renderPass()
{
    Q_ASSERT(mState && mState->renderPass);
    return *mState->renderPass;
}

KDGpu::Format VKWindow::swapchainFormat() const
{
    Q_ASSERT(mState);
    return mState->swapchainFormat;
}

KDGpu::Extent2D VKWindow::swapchainExtent() const
{
    Q_ASSERT(mState);
    return mState->swapchainExtent;
}

void VKWindow::initializeGpu()
{
    if (mInitialized)
        return;

    auto state = std::make_unique<State>();
    state->shared = sharedGpuContext();

    state->surface =
        state->shared->instance.createSurface(surfaceOptions(*this));
    if (!state->surface.isValid()) {
        qWarning() << "Creating KDGpu Vulkan window surface failed";
        return;
    }

    if (!state->shared->initializeDevice(state->surface))
        return;

    const auto swapchainProperties =
        state->shared->adapter->swapchainProperties(state->surface);
    if (swapchainProperties.formats.empty()
        || swapchainProperties.presentModes.empty()) {
        qWarning() << "KDGpu Vulkan surface has no swapchain support";
        return;
    }

    state->swapchainFormat =
        chooseSwapchainFormat(swapchainProperties.formats, &state->colorSpace);
    state->presentMode =
        choosePresentMode(swapchainProperties.presentModes, mSyncInterval != 0);
    state->compositeAlpha = chooseCompositeAlpha(
        swapchainProperties.capabilities.supportedCompositeAlpha);

    for (auto i = 0u; i < MaxFramesInFlight; ++i) {
        state->presentCompleteSemaphores[i] =
            state->shared->device.createGpuSemaphore();
        state->frameFences[i] = state->shared->device.createFence({
            .createSignalled = true,
        });
    }

    mState = std::move(state);
    mInitialized = true;
    Q_EMIT initializingGpu();
}

void VKWindow::releaseGpu()
{
    if (!mInitialized)
        return;

    if (mState && mState->shared->device.isValid())
        mState->shared->device.waitUntilIdle();

    Q_EMIT releasingGpu();

    mState.reset();
    mInitialized = false;
}

void VKWindow::paintGpu()
{
    if (!mInitialized)
        return;

    Q_EMIT paintingGpu();
}

bool VKWindow::event(QEvent *event)
{
    switch (event->type()) {
    case QEvent::UpdateRequest: update(); return true;
    case QEvent::Resize:
        if (mState)
            mState->swapchainDirty = true;
        requestUpdate();
        break;
    default: break;
    }
    return QWindow::event(event);
}

void VKWindow::exposeEvent(QExposeEvent *)
{
    if (isExposed())
        update();
}

void VKWindow::update()
{
    if (!isExposed())
        return;

    if (!mInitialized)
        initializeGpu();

    if (!initialized())
        return;

    swapBuffers();
}

void VKWindow::swapBuffers()
{
    if (!mState)
        return;

    auto &state = *mState;
    auto &shared = *state.shared;

    const auto recreateSwapchain = [&]() {
        const auto dpr = devicePixelRatio();
        const auto requestedWidth =
            std::max(1u, static_cast<uint32_t>(width() * dpr + 0.5));
        const auto requestedHeight =
            std::max(1u, static_cast<uint32_t>(height() * dpr + 0.5));

        const auto swapchainProperties =
            shared.adapter->swapchainProperties(state.surface);
        const auto &capabilities = swapchainProperties.capabilities;
        state.swapchainExtent = {
            .width = std::clamp(requestedWidth,
                capabilities.minImageExtent.width,
                capabilities.maxImageExtent.width),
            .height = std::clamp(requestedHeight,
                capabilities.minImageExtent.height,
                capabilities.maxImageExtent.height),
        };

        shared.device.waitUntilIdle();
        state.renderPass.reset();
        for (auto &commandBuffer : state.commandBuffers)
            commandBuffer = {};
        state.swapchainViews.clear();
        state.renderCompleteSemaphores.clear();

        state.swapchain = shared.device.createSwapchain({
            .surface = state.surface,
            .format = state.swapchainFormat,
            .colorSpace = state.colorSpace,
            .minImageCount =
                KDGpu::getSuitableImageCount(swapchainProperties.capabilities),
            .imageExtent = state.swapchainExtent,
            .imageUsageFlags = KDGpu::TextureUsageFlagBits::ColorAttachmentBit,
            .compositeAlpha = state.compositeAlpha,
            .presentMode = state.presentMode,
            .oldSwapchain = state.swapchain,
        });
        if (!state.swapchain.isValid()) {
            qWarning() << "Creating KDGpu Vulkan window swapchain failed";
            return false;
        }

        const auto &swapchainTextures = state.swapchain.textures();
        state.swapchainViews.reserve(swapchainTextures.size());
        state.renderCompleteSemaphores.reserve(swapchainTextures.size());
        for (const auto &texture : swapchainTextures) {
            state.swapchainViews.emplace_back(
                texture.createView({ .format = state.swapchainFormat }));
            state.renderCompleteSemaphores.emplace_back(
                shared.device.createGpuSemaphore());
        }

        state.swapchainDirty = false;
        return true;
    };

    if (state.swapchainDirty && !recreateSwapchain())
        return;

    auto &frameFence = state.frameFences[state.inFlightIndex];
    frameFence.wait();

    auto &presentComplete =
        state.presentCompleteSemaphores[state.inFlightIndex];
    const auto acquireResult = state.swapchain.getNextImageIndex(
        state.currentSwapchainImageIndex, presentComplete);
    if (acquireResult == KDGpu::AcquireImageResult::OutOfDate) {
        state.swapchainDirty = true;
        return;
    }
    if (acquireResult != KDGpu::AcquireImageResult::Success)
        return;

    frameFence.reset();
    auto &renderComplete =
        state.renderCompleteSemaphores.at(state.currentSwapchainImageIndex);

    auto commandRecorder =
        shared.device.createCommandRecorder({ .queue = shared.queue });
    state.renderPass.emplace(commandRecorder.beginRenderPass({
        .colorAttachments = {
            {
                .view =
                    state.swapchainViews.at(state.currentSwapchainImageIndex),
                .clearValue =
                    KDGpu::ColorClearValue{ 0.0f, 0.0f, 0.0f, 1.0f },
                .finalLayout = KDGpu::TextureLayout::PresentSrc,
            },
        },
        .framebufferWidth = state.swapchainExtent.width,
        .framebufferHeight = state.swapchainExtent.height,
    }));
    paintGpu();
    state.renderPass->end();
    state.renderPass.reset();

    state.commandBuffers[state.inFlightIndex] = commandRecorder.finish();

    shared.queue.submit({
        .commandBuffers = { state.commandBuffers[state.inFlightIndex] },
        .waitSemaphores = { presentComplete },
        .signalSemaphores = { renderComplete },
        .signalFence = frameFence,
    });

    const auto presentResult = shared.queue.present({
        .waitSemaphores = { renderComplete },
        .swapchainInfos = {
            {
                .swapchain = state.swapchain,
                .imageIndex = state.currentSwapchainImageIndex,
            },
        },
    });
    if (presentResult == KDGpu::PresentResult::OutOfDate
        || presentResult == KDGpu::PresentResult::SurfaceLost) {
        state.swapchainDirty = true;
    }

    state.inFlightIndex = (state.inFlightIndex + 1) % MaxFramesInFlight;
}
