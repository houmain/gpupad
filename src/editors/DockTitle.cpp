#include "DockTitle.h"
#include "../getMousePosition.h"
#include <QStylePainter>
#include <QStyleOption>
#include <QPaintEvent>
#include <QDockWidget>

namespace {
    const auto minimumTabWidth = 70;
    const auto targetTabWidth = 100;
} // namespace

DockTitle::DockTitle(QDockWidget *parent) 
    : QWidget(parent)
{
    setMouseTracking(true);
}

QSize DockTitle::sizeHint() const
{
    return (!mMinimumSize.isEmpty() ? mMinimumSize :
        calculateMinimumTabSize(tabText(0))); 
}

void DockTitle::setTabBar(QTabBar *tabBar)
{
    if (mTabBar != tabBar) {
        mTabBar = tabBar;
        update();
    }
}

int DockTitle::tabCount() const 
{
    return qMax(1, mTabBar ? mTabBar->count() : 0);
}

int DockTitle::currentTabIndex() const
{
    return (mTabBar && mTabBar->count() > 1 ? 
        mTabBar->currentIndex() : 0);
}

QString DockTitle::tabText(int index) const
{
    const auto dock = tabDock(index);
    auto title = dock->windowTitle();
    if (title.startsWith("[*]"))
        title.replace(0, 3, (dock->isWindowModified() ? "*" : ""));
    return title;
}

QRect DockTitle::tabRect(int index) const 
{
    auto rect = QRect(QPoint(), sizeHint());
    const auto count = tabCount();        
    for (auto i = 0; i <= index; ++i) {
        rect.setWidth(tabSize(i).width());
        if (i < index)
            rect.translate(rect.width(), 0);
    }
    return rect;
}

QDockWidget *DockTitle::tabDock(int index)
{
    return const_cast<QDockWidget *>(
        static_cast<const DockTitle*>(this)->tabDock(index));
}

const QDockWidget *DockTitle::tabDock(int index) const
{
    if (mTabBar && mTabBar->count() > 1) {
        // source: https://bugreports.qt.io/browse/QTBUG-39489
        return reinterpret_cast<QDockWidget*>(
            mTabBar->tabData(index).toULongLong());
    }
    return static_cast<QDockWidget*>(parentWidget());
}

QSize DockTitle::tabSize(int index) const
{
    return mTabSizes[index];
}

QSize DockTitle::calculateMinimumTabSize(const QString &text) const
{
    auto opt = QStyleOptionTab();
    opt.initFrom(this);
    auto contentSize = fontMetrics().size(Qt::TextShowMnemonic, text);
    contentSize += QSize(
        style()->pixelMetric(QStyle::PM_TabBarTabHSpace, &opt, this),
        style()->pixelMetric(QStyle::PM_TabBarTabVSpace, &opt, this));
    return style()->sizeFromContents(QStyle::CT_TabBarTab, &opt, contentSize, this);
}

void DockTitle::updateTabSizes() 
{
    auto minimumSizes = QList<QSize>();
    const auto count = tabCount();
    for (auto i = 0; i < count; ++i)
        minimumSizes.append(calculateMinimumTabSize(tabText(i)));

    auto minimumWidth = 0;
    auto widthGrowRequest = 0;
    for (const auto& size : qAsConst(minimumSizes)) {
        minimumWidth += size.width();
        widthGrowRequest += qMax(targetTabWidth - size.width(), 0);
    }
    mMinimumSize = { minimumWidth, minimumSizes[0].height() };
       
    const auto maximumWidth = width() - count;
    const auto widthLeft = qMax(maximumWidth - minimumWidth, 0);

    mTabSizes.clear();
    for (auto size : qAsConst(minimumSizes)) {
        if (auto grow = qMax(targetTabWidth - size.width(), 0))
            size.setWidth(size.width() + qMin(widthLeft * grow / widthGrowRequest, grow));
        if (auto shrink = qMax(minimumWidth - maximumWidth, 0) / count)
            size.setWidth(qMax(size.width() - shrink, minimumTabWidth));
        mTabSizes.append(size);
    }
}

int DockTitle::tabContainingPoint(const QPoint &point)
{
    auto rect = QRect(QPoint(), sizeHint());
    const auto count = tabCount();        
    for (auto i = 0; i < count; ++i) {
        rect.setWidth(tabSize(i).width());
        if (rect.contains(point))
            return i;
        rect.translate(rect.width(), 0);
    }
    return -1;
}

void DockTitle::mousePressEvent(QMouseEvent *event)
{
    const auto index = tabContainingPoint(getMousePosition(event));
    if (index >= 0) {
        auto dock = tabDock(index);
        if (event->button() == Qt::MiddleButton) {
            Q_EMIT dockCloseRequested(dock);
        }
        else if (event->button() == Qt::LeftButton) {
            if (index != currentTabIndex() || !dock->widget()->hasFocus()) {
                const auto switched = (index != currentTabIndex());
                dock->raise();
                dock->widget()->setFocus();

                // intercept event when when switching tabs, 
                // so it does not start dragging previous
                if (switched)
                    return;
            }
        }
    }
    QWidget::mousePressEvent(event);
}

void DockTitle::mouseReleaseEvent(QMouseEvent *event)
{
    QWidget::mouseReleaseEvent(event);

    if (event->button() == Qt::RightButton) {
        const auto index = tabContainingPoint(getMousePosition(event));
        if (index >= 0) {
            auto dock = tabDock(index);
            Q_EMIT contextMenuRequested(getGlobalMousePosition(event), mTabBar, dock);
        }
    }
}

void DockTitle::mouseDoubleClickEvent(QMouseEvent *event)
{
    const auto index = tabContainingPoint(getMousePosition(event));
    if (index == -1) {
        auto dock = tabDock(currentTabIndex());
        dock->raise();
        dock->widget()->setFocus();
    
        Q_EMIT openNewDock();
    }
}

void DockTitle::mouseMoveEvent(QMouseEvent *event) 
{
    const auto index = tabContainingPoint(getMousePosition(event));
    if (index >= 0) {
        auto dock = tabDock(index);
        const auto text = (dock->statusTip().isEmpty() ? 
            dock->windowTitle().replace("[*]", 
                dock->isWindowModified() ? "*" : "") :
            dock->statusTip());
        setToolTip(text);
    }
    else {
        setToolTip("");
    }
    QWidget::mouseMoveEvent(event);
}

void DockTitle::paintEvent(QPaintEvent *event) 
{
    updateTabSizes();

    auto rect = QRect(QPoint(), sizeHint());
    const auto current = currentTabIndex();
    const auto count = tabCount();        
    for (auto i = 0; i < count; ++i) {
        rect.setWidth(tabSize(i).width());
        paintTab(rect, tabText(i), i == current);
        rect.translate(rect.width(), 0);
    }
}

void DockTitle::paintTab(const QRect &rect, const QString &text, bool current)
{
    auto opt = QStyleOptionTab();
    opt.initFrom(this);
    opt.state = QStyle::State_Active | QStyle::State_Enabled;
    if (current) 
        opt.state |= QStyle::State_Selected;
           
    opt.rect = rect;
    // this is a hack to also fill space below tab
    opt.rect.setBottom(opt.rect.bottom() + 2);

    auto textRect = style()->subElementRect(QStyle::SE_TabBarTabText, &opt, this);
    opt.text = fontMetrics().elidedText(text,
        Qt::ElideRight, textRect.width(), Qt::TextShowMnemonic);
    auto painter = QStylePainter(this);
    painter.drawControl(QStyle::CE_TabBarTab, opt);
}
