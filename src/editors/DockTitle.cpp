#include "DockTitle.h"
#include <QDockWidget>
#include <QMenu>
#include <QPaintEvent>
#include <QStyleOption>
#include <QStylePainter>

namespace {
    const auto targetTabWidth = 100;
    const auto closeButtonSpace = 28;
    const auto closeButtonSize = 16;
    const auto tabsListButtonText = QChar(0x2026);
} // namespace

extern QDockWidget *getTabBarDock(QTabBar *tabBar, int index);

DockTitle::DockTitle(QDockWidget *parent) : QWidget(parent)
{
    setMouseTracking(true);
}

QSize DockTitle::sizeHint() const
{
    return (!mTabSizes.isEmpty() && !mTabSizes[0].isEmpty()
            ? mTabSizes[0]
            : calculateMinimumTabSize(tabText(0)));
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
    return (mTabBar && mTabBar->count() > 1 ? mTabBar->currentIndex() : 0);
}

QString DockTitle::tabText(int index) const
{
    const auto dock = tabDock(index);
    auto title = dock->windowTitle();
    if (title.startsWith("[*]"))
        title.replace(0, 3, (dock->isWindowModified() ? "*" : ""));
    return title;
}

QRect DockTitle::tabsRect() const
{
    return QRect(QPoint(0, 0), sizeHint().shrunkBy({ 0, 0, closeButtonSpace, 0 }));
}

QRect DockTitle::tabRect(int index) const
{
    auto rect = tabsRect();
    for (auto i = 0; i <= index; ++i) {
        rect.setWidth(tabSize(i).width());
        if (i < index)
            rect.translate(rect.width(), 0);
    }
    return rect;
}

QRect DockTitle::closeButtonRect() const
{
    const auto x = (closeButtonSpace - closeButtonSize) / 2;
    const auto y = (height() - closeButtonSize) / 2;
    return QRect(width() - closeButtonSpace + x, y, closeButtonSize, closeButtonSize);
}

QDockWidget *DockTitle::tabDock(int index)
{
    return const_cast<QDockWidget *>(
        static_cast<const DockTitle *>(this)->tabDock(index));
}

const QDockWidget *DockTitle::tabDock(int index) const
{
    if (mTabBar && mTabBar->count() > 1)
        return getTabBarDock(mTabBar, index);
    return static_cast<QDockWidget *>(parentWidget());
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
    contentSize +=
        QSize(style()->pixelMetric(QStyle::PM_TabBarTabHSpace, &opt, this),
            style()->pixelMetric(QStyle::PM_TabBarTabVSpace, &opt, this));
    return style()->sizeFromContents(QStyle::CT_TabBarTab, &opt, contentSize,
        this);
}

void DockTitle::updateTabSizes()
{
    mTabSizes.clear();
    auto &sizes = mTabSizes;

    if (mFloating) {
        sizes.append(size());
        return;
    }

    auto minimumWidth = 0;
    const auto count = tabCount();
    for (auto i = 0; i < count; ++i) {
        const auto size = calculateMinimumTabSize(tabText(i));
        sizes.append(size);
        minimumWidth += size.width();
    }

    mTabsListButtonWidth = 0;
    auto maximumWidth = width() - 1 - closeButtonSpace;
    if (count > 1 && minimumWidth > maximumWidth) {
        mTabsListButtonWidth =
            calculateMinimumTabSize(tabsListButtonText).width();
        maximumWidth -= mTabsListButtonWidth;

        // when width is exceeded, undo adding (non-current)
        // tabs to the end until they fit
        for (auto i = sizes.size() - 1; i >= 0; --i)
            if (i != currentTabIndex()) {
                minimumWidth -= sizes[i].width();
                sizes[i] = QSize{};
                if (minimumWidth <= maximumWidth)
                    break;
            }
    }

    // elide text, when single tab does not fit
    if (minimumWidth > maximumWidth) {
        sizes[currentTabIndex()].setWidth(maximumWidth);
        return;
    }

    // grow tabs to target size
    auto widthGrowRequest = 0;
    for (auto size : std::as_const(sizes))
        if (!size.isEmpty())
            widthGrowRequest += qMax(targetTabWidth - size.width(), 0);

    auto maxRoundingError = 0;
    const auto widthLeft = qMax(maximumWidth - minimumWidth, 0);
    if (widthLeft && widthGrowRequest)
        for (auto &size : sizes)
            if (!size.isEmpty())
                if (auto grow = qMax(targetTabWidth - size.width(), 0)) {
                    grow = qMin(widthLeft * grow / widthGrowRequest, grow);
                    size.setWidth(size.width() + grow);
                    minimumWidth += grow;
                    maxRoundingError += 1;
                }

    // add rounding errors to tab last tab size
    const auto rest = qMax(maximumWidth - minimumWidth, 0);
    if (rest < maxRoundingError)
        mTabSizes.back().setWidth(mTabSizes.back().width() + rest);
}

int DockTitle::tabContainingPoint(const QPoint &point, bool withTabListButton)
{
    auto rect = tabsRect();
    const auto count = tabCount();
    for (auto i = 0; i < count; ++i)
        if (const auto size = tabSize(i); !size.isEmpty()) {
            rect.setWidth(size.width());
            if (rect.contains(point))
                return i;
            rect.translate(size.width(), 0);
        }

    if (withTabListButton && mTabsListButtonWidth) {
        rect.setWidth(mTabsListButtonWidth);
        if (rect.contains(point))
            return count;
    }
    return -1;
}

void DockTitle::mousePressEvent(QMouseEvent *event)
{
    const auto index = tabContainingPoint(event->position().toPoint());
    if (event->button() == Qt::LeftButton &&
             closeButtonRect().contains(event->pos())) {
        mCloseButtonPressed = true;
        update(closeButtonRect());
    }
    else if (index >= 0) {
        auto dock = tabDock(index);
        if (event->button() == Qt::MiddleButton) {
            Q_EMIT dockCloseRequested(dock);
        } else if (event->button() == Qt::LeftButton) {
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
        QWidget::mousePressEvent(event);
    }
    mMousePressIntercepted = false;
}

void DockTitle::mouseReleaseEvent(QMouseEvent *event)
{
    if (mMousePressIntercepted)
        return;

    QWidget::mouseReleaseEvent(event);

    const auto index = tabContainingPoint(event->position().toPoint(), true);
    if (event->button() == Qt::LeftButton && index == tabCount()) {
        openTabsList(event->globalPosition().toPoint());
    } else if (event->button() == Qt::RightButton) {
        if (index >= 0 && index < tabCount()) {
            auto dock = tabDock(index);
            Q_EMIT contextMenuRequested(event->globalPosition().toPoint(),
                mTabBar, dock);
        }
    }
    else if (mCloseButtonPressed &&
             event->button() == Qt::LeftButton &&
             closeButtonRect().contains(event->pos())) {
        auto dock = tabDock(currentTabIndex());
        Q_EMIT dockCloseRequested(dock);
    }

    if (std::exchange(mCloseButtonPressed, false))
        update(closeButtonRect());
}

void DockTitle::mouseDoubleClickEvent(QMouseEvent *event)
{
    const auto index = tabContainingPoint(event->position().toPoint());
    if (event->button() == Qt::LeftButton && index == -1 &&
        !closeButtonRect().contains(event->pos())) {
        setCurrentTab(currentTabIndex());
        Q_EMIT openNewDock();
    } else if (event->button() == Qt::MiddleButton && index >= 0) {
        auto dock = tabDock(index);
        Q_EMIT dockCloseRequested(dock);
    }
}

void DockTitle::mouseMoveEvent(QMouseEvent *event)
{
    const auto index = tabContainingPoint(event->position().toPoint());
    if (index >= 0) {
        auto dock = tabDock(index);
        const auto text = (dock->statusTip().isEmpty()
                ? dock->windowTitle().replace("[*]",
                      dock->isWindowModified() ? "*" : "")
                : dock->statusTip());
        setToolTip(text);
    } else {
        setToolTip("");
    }

    const auto hovered = closeButtonRect().contains(event->pos());
    if (std::exchange(mCloseButtonHovered, hovered) != hovered)
      update(closeButtonRect());

    QWidget::mouseMoveEvent(event);
}

void DockTitle::leaveEvent(QEvent *event)
{
    if (std::exchange(mCloseButtonHovered, false))
      update(closeButtonRect());

    QWidget::leaveEvent(event);
}

void DockTitle::paintEvent(QPaintEvent *event)
{
    updateTabSizes();

    // collect visible tabs including tab list button
    struct Tab
    {
        QString text;
        QRect rect;
    };
    auto current = 0;
    const auto tabs = [&]() {
        const auto count = tabCount();
        auto tabs = QVector<Tab>();
        tabs.reserve(count + 1);

        auto rect = tabsRect();
        for (auto i = 0; i < count; ++i)
            if (!tabSize(i).isEmpty()) {
                rect.setWidth(tabSize(i).width());
                if (i == currentTabIndex())
                    current = tabs.size();
                tabs.append({ tabText(i), rect });
                rect.translate(rect.width(), 0);
            }

        if (mTabsListButtonWidth) {
            rect.setWidth(mTabsListButtonWidth);
            tabs.append({ tabsListButtonText, rect });
        }
        return tabs;
    }();

    // paint tabs, current last because it may overlap others
    auto painter = QStylePainter(this);
    const auto count = tabs.size();
    for (auto i = 0; i < count; ++i)
        if (i != current)
            paintTab(painter, tabs[i].rect, tabs[i].text, i, current, count - 1);

    paintTab(painter, tabs[current].rect, tabs[current].text, current,
        current, count - 1);

    paintCloseButton(painter);
}

void DockTitle::paintTab(QStylePainter &painter, const QRect &rect, const QString &text,
    int index, int current, int last)
{
    auto opt = QStyleOptionTab();
    opt.initFrom(this);
    opt.state = QStyle::State_Active | QStyle::State_Enabled;
    if (index == current)
        opt.state |= QStyle::State_Selected;

    opt.position = (last == 0 ? QStyleOptionTab::OnlyOneTab
            : index == 0      ? QStyleOptionTab::Beginning
            : index == last   ? QStyleOptionTab::End
                              : QStyleOptionTab::Middle);

    opt.selectedPosition = (current == index + 1
            ? QStyleOptionTab::NextIsSelected
            : current == index - 1 ? QStyleOptionTab::PreviousIsSelected
                                   : QStyleOptionTab::NotAdjacent);

    opt.rect = rect;
    // this is a hack to also fill space below tab (beneficial for Fusion style)
    opt.rect.setBottom(opt.rect.bottom() + 2);

    auto textRect =
        style()->subElementRect(QStyle::SE_TabBarTabText, &opt, this);
    opt.text = fontMetrics().elidedText(text, Qt::ElideRight, textRect.width(),
        Qt::TextShowMnemonic);
    painter.drawControl(QStyle::CE_TabBarTab, opt);
}

void DockTitle::paintCloseButton(QStylePainter &painter) {
    auto opt = QStyleOptionButton();
    opt.initFrom(this);
    opt.rect = closeButtonRect();
    opt.features = QStyleOptionButton::Flat;
    static QIcon icon{ ":/images/16x16/window-close.png" };
    opt.icon = icon;
    opt.iconSize = QSize(16, 16);
    opt.state = QStyle::State_Active | QStyle::State_Enabled;
    if (mCloseButtonHovered) {
        opt.state |= QStyle::State_MouseOver;
        if (mCloseButtonPressed)
            opt.state |= QStyle::State_On;
    }
    painter.drawControl(QStyle::CE_PushButton, opt);
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
