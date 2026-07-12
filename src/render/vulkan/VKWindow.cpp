#include "VKWindow.h"
#include "Singletons.h"
#include <QPlatformSurfaceEvent>
#include <QGuiApplication>
#include <KDGpu/surface_options.h>
#include <KDGpu/swapchain_options.h>

#if defined(Q_OS_LINUX)
#  include <QtGui/qguiapplication_platform.h>
#  if QT_VERSION < QT_VERSION_CHECK(6, 5, 0)
#    include <qpa/qplatformnativeinterface.h>
#  endif
#endif

#if defined(Q_OS_MACOS)
CAMetalLayer *gpupadCreateMetalLayer(QWindow &window);
void gpupadResizeMetalLayer(QWindow &window, uint32_t width, uint32_t height);
#endif

namespace {
    constexpr auto MaxFramesInFlight = 2u;

    KDGpu::SurfaceOptions surfaceOptions(QWindow &window)
    {
        auto options = KDGpu::SurfaceOptions{};
#if defined(Q_OS_WIN)
        options.hWnd = reinterpret_cast<HWND>(window.winId());
#elif defined(Q_OS_LINUX)
#  if QT_VERSION >= QT_VERSION_CHECK(6, 5, 0) && QT_CONFIG(wayland)
        if (auto *app = qApp
                ->nativeInterface<QNativeInterface::QWaylandApplication>()) {
            options.display = app->display();
            options.surface = reinterpret_cast<wl_surface *>(window.winId());
            if (options.display && options.surface)
                return options;
        }
#  else
        if (QGuiApplication::platformName().contains(QStringLiteral("wayland"),
                Qt::CaseInsensitive)) {
            if (auto *native = QGuiApplication::platformNativeInterface()) {
                options.display = static_cast<wl_display *>(
                    native->nativeResourceForIntegration(
                        QByteArrayLiteral("display")));
                options.surface =
                    static_cast<wl_surface *>(native->nativeResourceForWindow(
                        QByteArrayLiteral("surface"), &window));
                if (options.display && options.surface)
                    return options;
            }
        }
#  endif
#  if QT_CONFIG(xcb)
        if (auto *app =
                qApp->nativeInterface<QNativeInterface::QX11Application>()) {
            options.connection = app->connection();
            options.window = static_cast<xcb_window_t>(
                static_cast<std::uintptr_t>(window.winId()));
            return options;
        }
#  endif
#elif defined(Q_OS_MACOS)
        options.layer = gpupadCreateMetalLayer(window);
#else
#  error "not handled platform"
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
    VKDevice device;
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

QList<AdapterIdentity> VKWindow::getAdapterIdentities()
{
    auto device = VKDevice{};
    auto deviceLock = device.lock();
    auto &instance = deviceLock.instance();

    auto result = QList<AdapterIdentity>();
    for (const auto adapter : instance.adapters()) {
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

bool VKWindow::isSupported()
{
    auto device = VKDevice{};
    auto deviceLock = device.lock();
    auto &instance = deviceLock.instance();
    return instance.isValid();
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
    Q_ASSERT(!initialized());
}

VKDevice &VKWindow::device()
{
    Q_ASSERT(mState);
    return mState->device;
}

VKDevice::Lock VKWindow::lockDevice()
{
    return device().lock();
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
    auto deviceLock = state->device.lock();
    auto &instance = deviceLock.instance();
    if (!instance.isValid())
        return;

    if (!state->device.initialize(Singletons::selectedAdapter()))
        return;

    state->surface = instance.createSurface(surfaceOptions(*this));
    if (!state->surface.isValid())
        return;

    auto &adapter = deviceLock.adapter();
    auto &queue = deviceLock.queue();
    if (!adapter.supportsPresentation(state->surface, queue.queueTypeIndex()))
        return;

    const auto swapchainProperties =
        adapter.swapchainProperties(state->surface);
    if (swapchainProperties.formats.empty()
        || swapchainProperties.presentModes.empty())
        return;

    auto &device = deviceLock.device();
    state->swapchainFormat =
        chooseSwapchainFormat(swapchainProperties.formats, &state->colorSpace);
    state->presentMode =
        choosePresentMode(swapchainProperties.presentModes, mEnableVSync);
    state->compositeAlpha = chooseCompositeAlpha(
        swapchainProperties.capabilities.supportedCompositeAlpha);

    for (auto i = 0u; i < MaxFramesInFlight; ++i) {
        state->presentCompleteSemaphores[i] = device.createGpuSemaphore();
        state->frameFences[i] = device.createFence({
            .createSignalled = true,
        });
    }

    mState = std::move(state);
    Q_EMIT initializingGpu();
}

void VKWindow::releaseGpu()
{
    if (mState) {
        auto deviceLock = lockDevice();
        auto &device = deviceLock.device();
        device.waitUntilIdle();
        Q_EMIT releasingGpu();
    }
    mState.reset();
}

bool VKWindow::event(QEvent *event)
{
    switch (event->type()) {
    case QEvent::PlatformSurface:
        if (auto *surfaceEvent = static_cast<QPlatformSurfaceEvent *>(event);
            surfaceEvent->surfaceEventType()
            == QPlatformSurfaceEvent::SurfaceAboutToBeDestroyed)
            releaseGpu();
        break;
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

bool VKWindow::ensureSwapchain()
{
    auto &state = *mState;
    if (!state.swapchainDirty)
        return true;

    auto deviceLock = lockDevice();
    auto &adapter = deviceLock.adapter();
    auto &device = deviceLock.device();

    const auto dpr = devicePixelRatio();
    const auto requestedWidth =
        std::max(1u, static_cast<uint32_t>(width() * dpr + 0.5));
    const auto requestedHeight =
        std::max(1u, static_cast<uint32_t>(height() * dpr + 0.5));

#if defined(Q_OS_MACOS)
    gpupadResizeMetalLayer(*this, requestedWidth, requestedHeight);
#endif

    const auto swapchainProperties = adapter.swapchainProperties(state.surface);
    const auto &capabilities = swapchainProperties.capabilities;
    state.swapchainExtent = {
        .width = std::clamp(requestedWidth, capabilities.minImageExtent.width,
            capabilities.maxImageExtent.width),
        .height = std::clamp(requestedHeight,
            capabilities.minImageExtent.height,
            capabilities.maxImageExtent.height),
    };

    device.waitUntilIdle();
    state.renderPass.reset();
    for (auto &commandBuffer : state.commandBuffers)
        commandBuffer = {};
    state.swapchainViews.clear();
    state.renderCompleteSemaphores.clear();

    state.swapchain = device.createSwapchain({
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
            device.createGpuSemaphore());
    }

    state.swapchainDirty = false;
    return true;
}

void VKWindow::redraw()
{
    if (!isExposed())
        return;

    initializeGpu();
    if (!mState)
        return;

    auto deviceLock = lockDevice();
    if (!ensureSwapchain())
        return;

    auto &state = *mState;
    auto &frameFence = state.frameFences[state.inFlightIndex];
    frameFence.wait();

    auto &presentComplete =
        state.presentCompleteSemaphores[state.inFlightIndex];
    const auto acquireResult = state.swapchain.getNextImageIndex(
        state.currentSwapchainImageIndex, presentComplete);

    if (acquireResult == KDGpu::AcquireImageResult::SurfaceLost) {
        releaseGpu();
        return;
    }
    if (acquireResult == KDGpu::AcquireImageResult::OutOfDate) {
        state.swapchainDirty = true;
        return;
    }
    if (acquireResult != KDGpu::AcquireImageResult::Success
        && acquireResult != KDGpu::AcquireImageResult::SubOptimal)
        return;

    frameFence.reset();
    auto &renderComplete =
        state.renderCompleteSemaphores.at(state.currentSwapchainImageIndex);

    Q_EMIT preparingGpu();

    auto commandRecorder = deviceLock.device().createCommandRecorder({
        .queue = deviceLock.queue(),
    });
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

    deviceLock.queue().submit({
        .commandBuffers = { state.commandBuffers[state.inFlightIndex] },
        .waitSemaphores = { presentComplete },
        .signalSemaphores = { renderComplete },
        .signalFence = frameFence,
    });
    Q_EMIT submittedGpu();

    deviceLock.queue().present({
        .waitSemaphores = { renderComplete },
        .swapchainInfos = {
            {
                .swapchain = state.swapchain,
                .imageIndex = state.currentSwapchainImageIndex,
            },
        },
    });

    state.inFlightIndex = (state.inFlightIndex + 1) % MaxFramesInFlight;
}
