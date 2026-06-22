#include "VKWindow.h"
#include "VKDevice.h"
#include "Singletons.h"
#include <KDGpu/swapchain_options.h>

namespace {
    constexpr auto MaxFramesInFlight = 2u;

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
} // namespace

struct VKWindow::State
{
    std::shared_ptr<VKDevice> shared;
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
    return sharedVKDevice()->hasAdapters();
}

QList<AdapterIdentity> VKWindow::getAdapterIdentities()
{
    auto result = QList<AdapterIdentity>();
    const auto device = sharedVKDevice();
    if (!device->hasAdapters())
        return result;

    for (const auto adapter : device->instance().adapters()) {
        const auto &properties = adapter->properties();

        auto identity = AdapterIdentity{};
        identity.name = QString::fromStdString(properties.deviceName);
        std::copy(std::begin(properties.deviceUUID),
            std::end(properties.deviceUUID), identity.deviceUUIDs[0].begin());
        std::copy(std::begin(properties.driverUUID),
            std::end(properties.driverUUID), identity.driverUUID.begin());
        std::copy(std::begin(properties.deviceLUID),
            std::end(properties.deviceLUID), identity.deviceLUID.begin());
        result.append(identity);
    }
    return result;
}

//-------------------------------------------------------------------------

VKWindow::VKWindow(bool enableVSync, QWindow *parent)
    : QWindow(parent)
    , mEnableVSync(enableVSync)
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
    return mState->shared->device();
}

KDGpu::Queue &VKWindow::queue()
{
    Q_ASSERT(mState && mState->shared);
    return mState->shared->queue();
}

ktxVulkanDeviceInfo &VKWindow::ktxDeviceInfo()
{
    Q_ASSERT(mState && mState->shared);
    return mState->shared->ktxDeviceInfo();
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
    if (mState)
        return;

    auto state = std::make_unique<State>();
    state->shared = sharedVKDevice();
    if (!state->shared->hasAdapters())
        return;

    state->surface =
        state->shared->instance().createSurface(surfaceOptions(*this));
    if (!state->surface.isValid())
        return;

    if (!state->shared->initialize(state->surface,
            Singletons::selectedAdapter()))
        return;

    const auto swapchainProperties =
        state->shared->adapter().swapchainProperties(state->surface);
    if (swapchainProperties.formats.empty()
        || swapchainProperties.presentModes.empty())
        return;

    state->swapchainFormat =
        chooseSwapchainFormat(swapchainProperties.formats, &state->colorSpace);
    state->presentMode =
        choosePresentMode(swapchainProperties.presentModes, mEnableVSync);
    state->compositeAlpha = chooseCompositeAlpha(
        swapchainProperties.capabilities.supportedCompositeAlpha);

    for (auto i = 0u; i < MaxFramesInFlight; ++i) {
        state->presentCompleteSemaphores[i] =
            state->shared->device().createGpuSemaphore();
        state->frameFences[i] = state->shared->device().createFence({
            .createSignalled = true,
        });
    }

    mState = std::move(state);
    Q_EMIT initializingGpu();
}

void VKWindow::releaseGpu()
{
    if (mState) {
        mState->shared->device().waitUntilIdle();
        Q_EMIT releasingGpu();
    }
    mState.reset();
}

bool VKWindow::event(QEvent *event)
{
    switch (event->type()) {
    case QEvent::UpdateRequest: redraw(); return true;
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
    redraw();
}

void VKWindow::redraw()
{
    if (!isExposed())
        return;

    initializeGpu();
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
            shared.adapter().swapchainProperties(state.surface);
        const auto &capabilities = swapchainProperties.capabilities;
        state.swapchainExtent = {
            .width = std::clamp(requestedWidth,
                capabilities.minImageExtent.width,
                capabilities.maxImageExtent.width),
            .height = std::clamp(requestedHeight,
                capabilities.minImageExtent.height,
                capabilities.maxImageExtent.height),
        };

        shared.device().waitUntilIdle();
        state.renderPass.reset();
        for (auto &commandBuffer : state.commandBuffers)
            commandBuffer = {};
        state.swapchainViews.clear();
        state.renderCompleteSemaphores.clear();

        state.swapchain = shared.device().createSwapchain({
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
        if (!state.swapchain.isValid())
            return false;

        const auto &swapchainTextures = state.swapchain.textures();
        state.swapchainViews.reserve(swapchainTextures.size());
        state.renderCompleteSemaphores.reserve(swapchainTextures.size());
        for (const auto &texture : swapchainTextures) {
            state.swapchainViews.emplace_back(
                texture.createView({ .format = state.swapchainFormat }));
            state.renderCompleteSemaphores.emplace_back(
                shared.device().createGpuSemaphore());
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

    Q_EMIT preparingGpu();

    auto commandRecorder =
        shared.device().createCommandRecorder({ .queue = shared.queue() });
    state.renderPass.emplace(commandRecorder.beginRenderPass(
        KDGpu::RenderPassCommandRecorderOptions{
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

    Q_EMIT paintingGpu();

    state.renderPass->end();
    state.renderPass.reset();

    state.commandBuffers[state.inFlightIndex] = commandRecorder.finish();

    shared.queue().submit({
        .commandBuffers = { state.commandBuffers[state.inFlightIndex] },
        .waitSemaphores = { presentComplete },
        .signalSemaphores = { renderComplete },
        .signalFence = frameFence,
    });
    Q_EMIT submittedGpu();

    const auto presentResult = shared.queue().present({
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
