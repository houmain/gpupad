#ifndef DOCKWINDOW_H
#define DOCKWINDOW_H

#include <QMainWindow>

class DockWindow : public QMainWindow
{
    Q_OBJECT
public:
    explicit DockWindow(QWidget *parent = nullptr);
    ~DockWindow() override;

Q_SIGNALS:
    void openNewDock();

protected:
    void raiseDock(QDockWidget *dock);
    virtual bool closeDock(QDockWidget *dock, bool promptSave = true);

    QSize sizeHint() const override { return QSize(700, 800); }
    bool event(QEvent *event) override;
    bool eventFilter(QObject *watched, QEvent *event) override;

private:
    void tabbedDockClicked(int index);
    void tabBarDoubleClicked(int index);
    void openContextMenu(int index);
    void closeDocksExcept(QTabBar *tabBar, int index);
    void updateDocks();
    void updateTabBar(QTabBar *tabBar);

    QTimer *mUpdateDocksTimer{ };
    QList<QMetaObject::Connection> mDockConnections;
};

#endif // DOCKWINDOW_H
