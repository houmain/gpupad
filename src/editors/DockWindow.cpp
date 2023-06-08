#include "DockWindow.h"
#include "FileDialog.h"
#include "DockTitle.h"
#include <QChildEvent>
#include <QTabBar>
#include <QApplication>
#include <QTimer>
#include <QDockWidget>
#include <QPointer>
#include <QMenu>
#include <QClipboard>

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
}

void DockWindow::raiseDock(QDockWidget *dock)
{
    // it seems like raising only works when the dock was layouted
    QTimer::singleShot(0,
        [dock = QPointer<QDockWidget>(dock)]() {
            if (dock) {
                dock->raise();
                dock->widget()->setFocus();
            }
        });
}

void DockWindow::closeDock(QDockWidget *dock)
{
    dock->setParent(nullptr);
    dock->deleteLater();
}

void DockWindow::closeDocksExcept(QTabBar *tabBar, QDockWidget *except)
{
    for (auto i = tabBar->count() - 1; i >= 0; --i)
        if (auto dock = getTabBarDock(tabBar, i))
            if (dock != except)
                Q_EMIT dockCloseRequested(dock);
  
}

void DockWindow::childEvent(QChildEvent *event)
{
    // install event handler for added children (QTabBar, QDockWidget)
    if (event->added() && event->child()->isWidgetType())
        event->child()->installEventFilter(this);
}

bool DockWindow::eventFilter(QObject *watched, QEvent *event)
{
    // hide QTabBar automatically added by QMainWindow as soon as possible
    if (event->type() == QEvent::ChildPolished)
        if (auto tabBar = qobject_cast<QTabBar*>(watched))
            initializeTabBar(tabBar);
        
    // set custom titlebar of QDockWidget as soon as possible
    if (event->type() == QEvent::Polish)
        if (auto dock = qobject_cast<QDockWidget*>(watched))
            initializeDock(dock);
    
    // inform titlebars of corresponding tabbar, whenever tabbar children change
    if (event->type() == QEvent::ChildAdded || event->type() == QEvent::ChildRemoved)
        if (qobject_cast<QTabBar*>(watched)) {
            for (auto title : findChildren<DockTitle*>())
                title->setTabBar(nullptr);

            for (auto tabBar : findChildren<QTabBar*>())
                for (auto i = 0; i < tabBar->count(); i++)
                    if (auto dock = qobject_cast<QDockWidget*>(getTabBarDock(tabBar, i)))
                        if (auto title = qobject_cast<DockTitle*>(dock->titleBarWidget()))
                            title->setTabBar(tabBar);
        }

    return QMainWindow::eventFilter(watched, event);
}

void DockWindow::initializeTabBar(QTabBar *tabBar)
{
    if (!tabBar->property("initialized").isNull())
        return;
    tabBar->setProperty("initialized", true);

    tabBar->setStyleSheet("QTabBar::tab { max-height:0px }");
}

void DockWindow::initializeDock(QDockWidget *dock)
{
    if (!dock->property("initialized").isNull())
        return;
    dock->setProperty("initialized", true);

    connect(dock, &QDockWidget::topLevelChanged, 
        this, &DockWindow::onDockTopLevelChanged);
    setDockTitleBar(dock);
}

void DockWindow::openContextMenu(QPoint pos, QTabBar *tabBar, QDockWidget *dock)
{
    auto menu = QMenu();
    auto close = menu.addAction(tr("Close"));
    close->setIcon(QIcon::fromTheme("process-stop"));
    close->setShortcut(QKeySequence::Close);
    connect(close, &QAction::triggered, 
        [=]() { Q_EMIT dockCloseRequested(dock); });
    if (tabBar && tabBar->count() > 1) {
        auto closeAll = menu.addAction(tr("Close All"));
        auto closeOthers = menu.addAction(tr("Close Others"));
        connect(closeOthers, &QAction::triggered, 
            [=]() { closeDocksExcept(tabBar, dock); });
        connect(closeAll, &QAction::triggered, 
            [=]() { closeDocksExcept(tabBar, nullptr); });
    }

    if (!dock->statusTip().isEmpty()) {
        const auto fileName = dock->statusTip();
        auto copyFullPath = menu.addAction(tr("Copy Full Path"));
        auto openContainingFolder = menu.addAction(tr("Open Containing Folder"));
        connect(copyFullPath, &QAction::triggered,
            [=]() { QApplication::clipboard()->setText(fileName); });
        connect(openContainingFolder, &QAction::triggered,
            [=]() { showInFileManager(fileName); });
    }

    if (tabBar && tabBar->count() > 1) {
        menu.addSeparator();

        auto docks = std::multimap<QString, QDockWidget *>();
        for (auto i = 0; i < tabBar->count(); ++i)
            if (auto dock = getTabBarDock(tabBar, i)) {
                const auto title = (dock->statusTip().isEmpty() ? 
                    dock->windowTitle().replace("[*]", 
                        dock->isWindowModified() ? "*" : "") :
                    dock->statusTip());
                docks.emplace(title, dock);
            }

        for (const auto &[title, dock] : docks)
            connect(menu.addAction(title), &QAction::triggered, 
                [this, dock = dock]() { raiseDock(dock); });
    }
    menu.exec(pos);
}

void DockWindow::onDockTopLevelChanged(bool floating)
{
    auto dock = qobject_cast<QDockWidget*>(sender());
    setDockTitleBar(dock);

    auto size = dock->size();
    size.setWidth(static_cast<int>(size.width() * 0.9));
    size.setHeight(static_cast<int>(size.height() * 0.9));
    dock->resize(size.expandedTo(QSize(300, 300)));
}

void DockWindow::setDockTitleBar(QDockWidget *dock) {
    if (dock->isFloating()) {
        delete dock->titleBarWidget();
        dock->setTitleBarWidget(nullptr);
    }
    else if (!dock->titleBarWidget()) {
        auto title = new DockTitle(dock);
        dock->setTitleBarWidget(title);

        connect(title, &DockTitle::openNewDock,
            this, &DockWindow::openNewDock);
        connect(title, &DockTitle::dockCloseRequested,
            this, &DockWindow::dockCloseRequested);
        connect(title, &DockTitle::contextMenuRequested,
            this, &DockWindow::openContextMenu);
    }
}
