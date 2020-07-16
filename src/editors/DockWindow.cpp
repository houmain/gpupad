#include "DockWindow.h"
#include <QTabBar>
#include <QApplication>
#include <QTimer>
#include <QDockWidget>
#include <QPointer>

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

bool DockWindow::closeDock(QDockWidget *dock)
{
    delete dock;
    return true;
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

void DockWindow::tabbedDockChanged(int index)
{
    auto tabBar = static_cast<QTabBar*>(QObject::sender());
    if (auto dock = getTabBarDock(tabBar, index))
        dock->widget()->setFocus();
}

void DockWindow::tabbedDockClicked(int index)
{
    if (QApplication::mouseButtons() & Qt::MiddleButton) {
        auto tabBar = static_cast<QTabBar*>(QObject::sender());
        if (auto dock = getTabBarDock(tabBar, index))
            closeDock(dock);
    }
}

void DockWindow::updateDocks()
{
    for (const auto &connection : qAsConst(mDockConnections))
       disconnect(connection);
    mDockConnections.clear();

    for (QTabBar* tabBar : findChildren<QTabBar*>())
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
        connect(tabBar, &QTabBar::currentChanged,
            this, &DockWindow::tabbedDockChanged);
}
