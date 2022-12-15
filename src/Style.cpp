
#include "Style.h"
#include <QStyleFactory>
#include <QPainter>

Style::Style()
    : QProxyStyle(QStyleFactory::create("Fusion"))
    , mDockClose(":images/dock-close.png")
    , mDockRestore(":images/dock-restore.png")
{

}

QIcon Style::standardIcon(QStyle::StandardPixmap standardIcon,
                                    const QStyleOption *option,
                                    const QWidget *widget) const
{
    switch (standardIcon) {
        case SP_DockWidgetCloseButton:
        case SP_TitleBarCloseButton:
            return mDockClose;
        case SP_TitleBarNormalButton:
            return mDockRestore;
        default:
            return QProxyStyle::standardIcon(standardIcon, option, widget);
    }
}

QPixmap Style::generatedIconPixmap(QIcon::Mode iconMode, const QPixmap &pixmap,
        const QStyleOption *opt) const
{
    if (iconMode == QIcon::Mode::Disabled) {
        auto dark = pixmap;
        QPainter p(&dark);
        p.setCompositionMode(QPainter::CompositionMode_DestinationIn);
        p.fillRect(dark.rect(), QColor(0, 0, 0, 128));
        return dark;
    }
    return pixmap;
}
