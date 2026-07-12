#include <QWindow>

#include <algorithm>
#include <cstdint>

#import <AppKit/NSView.h>
#import <QuartzCore/CAMetalLayer.h>

namespace {
    NSView *nativeView(QWindow &window)
    {
        return reinterpret_cast<NSView *>(window.winId());
    }

    uint32_t pixelSize(int logicalSize, qreal devicePixelRatio)
    {
        return std::max(1u,
            static_cast<uint32_t>(logicalSize * devicePixelRatio + 0.5));
    }
} // namespace

void gpupadResizeMetalLayer(QWindow &window, uint32_t width, uint32_t height)
{
    auto *view = nativeView(window);
    if (!view)
        return;

    CALayer *layer = [view layer];
    if (![layer isKindOfClass:[CAMetalLayer class]])
        return;

    auto *metalLayer = (CAMetalLayer *)layer;
    metalLayer.contentsScale = window.devicePixelRatio();
    metalLayer.drawableSize = CGSizeMake(width, height);
}

CAMetalLayer *gpupadCreateMetalLayer(QWindow &window)
{
    auto *view = nativeView(window);
    if (!view)
        return nullptr;

    [view setWantsLayer:YES];

    CALayer *layer = [view layer];
    CAMetalLayer *metalLayer = nullptr;
    if ([layer isKindOfClass:[CAMetalLayer class]]) {
        metalLayer = (CAMetalLayer *)layer;
    } else {
        metalLayer = [CAMetalLayer layer];
        if (!metalLayer)
            return nullptr;
        [view setLayer:metalLayer];
    }

    metalLayer.framebufferOnly = YES;
    const auto dpr = window.devicePixelRatio();
    gpupadResizeMetalLayer(window, pixelSize(window.width(), dpr),
        pixelSize(window.height(), dpr));
    return metalLayer;
}
