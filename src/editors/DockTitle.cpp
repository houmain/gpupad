#include "DockTitle.h"
#include "../getMousePosition.h"
#include <QStylePainter>
#include <QStyleOption>
#include <QPaintEvent>
#include <QDockWidget>
#include <QMenu>

namespace {
    const auto targetTabWidth = 100;
    const auto tabsListButtonWidth = 30;
} // namespace

DockTitle::DockTitle(QDockWidget *parent) 
    : QWidget(parent)
{
    setMouseTracking(true);
}

QSize DockTitle::sizeHint() const
{
    return (!mSizeHint.isEmpty() ? mSizeHint :
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
    return (index < mTabSizes.size() ? mTabSizes[index] : QSize());
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
    mTabSizes.clear();
    auto &sizes = mTabSizes;

    auto minimumWidth = 0;
    const auto count = tabCount();
    for (auto i = 0; i < count; ++i) {
        const auto size = calculateMinimumTabSize(tabText(i));
        sizes.append(size);
        minimumWidth += size.width();
    }

    mShowTabsListButton = false;
    auto maximumWidth = width() - 1;
    if (count > 1 && minimumWidth > maximumWidth) {
        mShowTabsListButton = true;
        maximumWidth -= tabsListButtonWidth;

        // when width is exceeded, undo adding (non-current) 
        // tabs to the end until they fit
        for (auto i = sizes.size() - 1; i >= 0; --i) 
            if (i != currentTabIndex()) {
                minimumWidth -= sizes[i].width();
                sizes[i] = QSize{ };
                if (minimumWidth <= maximumWidth)
                    break;
            }
    }

    mSizeHint = { minimumWidth, sizes[0].height() };

    // elide text, when single tab does not fit
    if (minimumWidth > maximumWidth) {
        sizes[currentTabIndex()].setWidth(maximumWidth);
        return;
    }

    // grow tabs to target size
    auto widthGrowRequest = 0;
    for (auto size : qAsConst(sizes)) 
        if (!size.isEmpty())
            widthGrowRequest += qMax(targetTabWidth - size.width(), 0);

    const auto widthLeft = qMax(maximumWidth - minimumWidth, 0);
    if (widthLeft)
        for (auto &size : sizes)
            if (!size.isEmpty())
                if (auto grow = qMax(targetTabWidth - size.width(), 0)) {
                    grow = qMin(widthLeft * grow / widthGrowRequest, grow);
                    size.setWidth(size.width() + grow);
                    minimumWidth += grow;
                }

    // add rounding errors to current tab size
    if (widthGrowRequest > widthLeft) {
        auto &size = mTabSizes[currentTabIndex()];
        size.setWidth(size.width() + (maximumWidth - minimumWidth));
    }
}

int DockTitle::tabContainingPoint(const QPoint &point)
{
    auto rect = QRect(QPoint(), sizeHint());
    const auto count = tabCount();        
    for (auto i = 0; i < count; ++i) 
        if (const auto size = tabSize(i); !size.isEmpty()) {
            rect.setWidth(size.width());
            if (rect.contains(point))
                return i;
            rect.translate(size.width(), 0);
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
                setCurrentTab(index);

                // intercept event when when switching tabs, 
                // so it does not start dragging previous
                if (switched) {
                    mMousePressIntercepted = true;
                    return;
                }
            }
        }
    }
    mMousePressIntercepted = false;
    QWidget::mousePressEvent(event);
}

void DockTitle::mouseReleaseEvent(QMouseEvent *event)
{
    if (mMousePressIntercepted)
        return;

    QWidget::mouseReleaseEvent(event);

    const auto index = tabContainingPoint(getMousePosition(event));
    if (event->button() == Qt::LeftButton && mShowTabsListButton && index == -1) {
        openTabsList(getGlobalMousePosition(event));
    }
    else if (event->button() == Qt::RightButton) {
        if (index >= 0) {
            auto dock = tabDock(index);
            Q_EMIT contextMenuRequested(getGlobalMousePosition(event), mTabBar, dock);
        }
    }
}

void DockTitle::mouseDoubleClickEvent(QMouseEvent *event)
{
    const auto index = tabContainingPoint(getMousePosition(event));
    if (event->button() == Qt::LeftButton && index == -1) {
        setCurrentTab(currentTabIndex());
        Q_EMIT openNewDock();
    }
    else if (event->button() == Qt::MiddleButton && index >= 0) {
        auto dock = tabDock(index);
        Q_EMIT dockCloseRequested(dock);
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
    for (auto i = 0; i < count; ++i)
        if (!tabSize(i).isEmpty()) {
            rect.setWidth(tabSize(i).width());
            paintTab(rect, tabText(i), i == current);
            rect.translate(rect.width(), 0);
        }

    if (mShowTabsListButton) {
        rect.setWidth(tabsListButtonWidth);
        paintTab(rect, QChar(0x2026), false);
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

void DockTitle::setCurrentTab(int index) 
{
    if (auto dock = tabDock(index)) {
        dock->raise();
        dock->widget()->setFocus();
    }
}

void DockTitle::openTabsList(const QPoint &position)
{
    auto menu = QMenu();
    for (auto i = 0; i < tabCount(); ++i)
        if (tabRect(i).isEmpty())
            connect(menu.addAction(tabText(i)), &QAction::triggered,
                [this, i]() { setCurrentTab(i); });

    menu.exec(position);
}
