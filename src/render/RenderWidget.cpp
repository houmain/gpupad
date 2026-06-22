
#include "RenderWidget.h"
#include "render/vulkan/VKWindow.h"
#include "render/opengl/GLWindow.h"

RenderWidget::RenderWidget(bool enableVSync, QWidget *parent)
    : WindowWidget(true, parent)
    , mEnableVSync(enableVSync)
{
}

void RenderWidget::setAdapter(const AdapterIdentity &adapterIdentity)
{
    if (mAdapterIdentity == adapterIdentity && widgetWindow())
        return;
    mAdapterIdentity = adapterIdentity;

#if defined(VULKAN_ENABLED)
    if (VKWindow::isSupported())
        return setWidgetWindow(new VKWindow(mEnableVSync));
#endif

#if defined(OPENGL_ENABLED)
    return setWidgetWindow(new GLWindow(mEnableVSync));
#endif
}

void RenderWidget::redraw()
{
#if defined(VULKAN_ENABLED)
    if (auto vkWindow = qobject_cast<VKWindow *>(widgetWindow()))
        return vkWindow->redraw();
#endif

#if defined(OPENGL_ENABLED)
    if (auto glWindow = qobject_cast<GLWindow *>(widgetWindow()))
        return glWindow->redraw();
#endif
}
