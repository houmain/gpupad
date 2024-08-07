#pragma once

#include <QWidget>

class QTabBar;
class QDockWidget;

class DockTitle : public QWidget
{
    Q_OBJECT
public:
    explicit DockTitle(QDockWidget *parent);

    void setTabBar(QTabBar *tabBar);
    QSize sizeHint() const override;

Q_SIGNALS:
    void openNewDock();
    void dockCloseRequested(QDockWidget *dock);
    void contextMenuRequested(QPoint pos, QTabBar *tabBar, QDockWidget *dock);

protected:
    void paintEvent(QPaintEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseDoubleClickEvent(QMouseEvent *event) override;

private:
    void paintTab(const QRect &rect, const QString &text, int index,
        int current, int last);
    int tabCount() const;
    int currentTabIndex() const;
    int tabContainingPoint(const QPoint &point);
    QSize tabSize(int index) const;
    QString tabText(int index) const;
    QRect tabRect(int index) const;
    QDockWidget *tabDock(int index);
    const QDockWidget *tabDock(int index) const;
    void updateTabSizes();
    QSize calculateMinimumTabSize(const QString &text) const;
    void setCurrentTab(int index);
    void openTabsList(const QPoint &position);

    QTabBar *mTabBar{};
    QList<QSize> mTabSizes;
    int mTabsListButtonWidth{};
    bool mMousePressIntercepted{};
};
