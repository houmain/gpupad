#pragma once

#include <QWidget>
#include <QWindow>
#include <QTimer>

class WindowWidget : public QWidget
{
    Q_OBJECT
public:
    explicit WindowWidget(bool forwardInputEvents, QWidget *parent = nullptr);
    explicit WindowWidget(QWidget *parent) = delete;

    QWindow *widgetWindow() const { return mWindow; }
    void setWidgetWindow(QWindow *window);
    void resizeEvent(QResizeEvent *event) override;
    void showEvent(QShowEvent *event) override;

private:
    bool eventFilter(QObject *watched, QEvent *event) override;
    void scheduleResize();
    void resizeWindow();

    const bool mForwardInputEvents;
    QTimer mResizeTimer;
    QWindow *mWindow{};
    QWidget *mWindowContainer{};
};
