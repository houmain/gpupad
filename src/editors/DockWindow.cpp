#include "DockWindow.h"
#include <QTabBar>
#include <QApplication>
#include <QTimer>
#include <QDockWidget>
#include <QPointer>
#include <QMenu>

namespace {
    QDockWidget *getTabBarDock(QTabBar *tabBar, int index)
    {
        // source: https://bugreports.qt.io/browse/QTBUG-39489
        return reinterpret_cast<QDockWidget*>(
            tabBar->tabData(index).toULongLong());
    }
} // namespace

DockWindow::DockWindow(QWidget *parent)
    : QMainWindow(parent)
{
    mUpdateDocksTimer = new QTimer(this);
}

DockWindow::~DockWindow()
{
}

void DockWindow::raiseDock(QDockWidget *dock)
{
    // it seems like raising only works when the dock was layouted
    mUpdateDocksTimer->singleShot(0,
        [dock = QPointer<QDockWidget>(dock)]() {
            if (dock) {
                dock->raise();
                dock->widget()->setFocus();
            }
        });
}

bool DockWindow::closeDock(QDockWidget *dock, bool promptSave)
{
    delete dock;
    return true;
}

void DockWindow::closeDocksExcept(QTabBar *tabBar, int index)
{
    for (auto i = tabBar->count() - 1; i >= 0; --i)
        if (i != index)
            if (auto dock = getTabBarDock(tabBar, i))
                closeDock(dock);
}

bool DockWindow::event(QEvent *event)
{
    if (mUpdateDocksTimer && !mUpdateDocksTimer->isActive()) {
        if (event->type() == QEvent::ChildAdded ||
            event->type() == QEvent::ChildRemoved) {
            mUpdateDocksTimer->singleShot(0,
                this, &DockWindow::updateDocks);
        }
    }
    return QMainWindow::event(event);
}

bool DockWindow::eventFilter(QObject *watched, QEvent *event)
{
    if (event->type() == QEvent::Close)
        if (auto dock = qobject_cast<QDockWidget*>(watched)) {
            if (!closeDock(dock))
                event->ignore();
            return true;
        }
    return QMainWindow::eventFilter(watched, event);
}

void DockWindow::tabbedDockClicked(int index)
{
    auto tabBar = static_cast<QTabBar*>(QObject::sender());
    if (auto dock = getTabBarDock(tabBar, index)) {
        if (QApplication::mouseButtons() & Qt::LeftButton) {
            dock->widget()->setFocus();
        }
        else if (QApplication::mouseButtons() & Qt::MiddleButton) {
            closeDock(dock);
        }
        else if (QApplication::mouseButtons() & Qt::RightButton) {
            openContextMenu(index);
        }
    }
}

void DockWindow::openContextMenu(int index)
{
    auto tabBar = static_cast<QTabBar*>(QObject::sender());
    if (auto dock = getTabBarDock(tabBar, index)) {
        auto menu = QMenu();
        auto close = menu.addAction(tr("Close"));
        close->setIcon(QIcon(QString::fromUtf8(":/images/16x16/window-close.png")));
        close->setShortcut(QKeySequence::Close);
        connect(close, &QAction::triggered, 
            [=]() { closeDock(dock); });
        if (tabBar->count() > 1) {
            auto closeAll = menu.addAction(tr("Close All Tabs"));
            auto closeOthers = menu.addAction(tr("Close All But This"));
            connect(closeOthers, &QAction::triggered, 
                [=]() { closeDocksExcept(tabBar, index); });
            connect(closeAll, &QAction::triggered, 
                [=]() { closeDocksExcept(tabBar, -1); });
        }
        menu.exec(tabBar->mapToGlobal(
            tabBar->tabRect(index).bottomLeft() + QPoint(0, 2)));
    }
}

void DockWindow::tabBarDoubleClicked(int index) 
{
    auto tabBar = static_cast<QTabBar*>(QObject::sender());
    if (index == -1) {
        if (QApplication::mouseButtons() & Qt::LeftButton) {
            Q_EMIT openNewDock();
        }
    }
}

void DockWindow::updateDocks()
{
    for (const auto &connection : qAsConst(mDockConnections))
       disconnect(connection);
    mDockConnections.clear();

    const auto children = findChildren<QTabBar*>();
    for (QTabBar* tabBar : children)
        updateTabBar(tabBar);
}

void DockWindow::updateTabBar(QTabBar *tabBar)
{
    tabBar->setUsesScrollButtons(true);
    tabBar->setElideMode(Qt::ElideLeft);

    QCoreApplication::setAttribute(Qt::AA_UseStyleSheetPropagationInWidgetStyles, true);
    tabBar->setStyleSheet(
        "QTabBar::tab { min-width:100px; max-width: 200px; }");

    mDockConnections +=
        connect(tabBar, &QTabBar::tabBarClicked,
            this, &DockWindow::tabbedDockClicked);
    mDockConnections +=
        connect(tabBar, &QTabBar::tabBarDoubleClicked,
            this, &DockWindow::tabBarDoubleClicked);
}
