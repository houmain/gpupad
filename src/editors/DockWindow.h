#pragma once

#include <QMainWindow>

class DockWindow : public QMainWindow
{
    Q_OBJECT
public:
    explicit DockWindow(QWidget *parent = nullptr);
  
    void raiseDock(QDockWidget *dock);
    void closeDock(QDockWidget *dock);
    void closeDocksExcept(QTabBar *tabBar, QDockWidget *dock);

    bool eventFilter(QObject *watched, QEvent *event) override;
  
Q_SIGNALS:
    void openNewDock();
    void dockCloseRequested(QDockWidget *dock);

private Q_SLOTS:
    void openContextMenu(QPoint pos, QTabBar *tabBar, QDockWidget *dock);
    void onDockTopLevelChanged(bool topLevel);

protected:
    void childEvent(QChildEvent *event) override;
   
private:
    void initializeTabBar(QTabBar *tabBar);
    void initializeDock(QDockWidget *dock);
    void setDockTitleBar(QDockWidget *dock);
};
