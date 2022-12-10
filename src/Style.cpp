
#include "Style.h"
#include <QStyleFactory>

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
