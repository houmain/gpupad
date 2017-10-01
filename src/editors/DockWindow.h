#ifndef DOCKWINDOW_H
#define DOCKWINDOW_H

#include <QMainWindow>

class DockWindow : public QMainWindow
{
    Q_OBJECT
public:
    explicit DockWindow(QWidget *parent = 0);
    ~DockWindow();

protected:
    virtual bool closeDock(QDockWidget *dock);

    QSize sizeHint() const override { return QSize(700, 800); }
    bool event(QEvent *event) override;
    bool eventFilter(QObject *watched, QEvent *event) override;

private slots:
    void tabbedDockChanged(int index);
    void tabbedDockClicked(int index);
    void updateDocks();

private:
    void updateTabBar(QTabBar *tabBar);

    QTimer *mUpdateDocksTimer{ };
    QList<QMetaObject::Connection> mDockConnections;
};

#endif // DOCKWINDOW_H
