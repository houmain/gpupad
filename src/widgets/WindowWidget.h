#pragma once

#include <QWidget>
#include <QWindow>
#include <QTimer>

class WindowWidget : public QWidget
{
    Q_OBJECT
public:
    explicit WindowWidget(QWidget *parent = nullptr);
    void setWindow(QWindow *window);
    void resizeEvent(QResizeEvent *event) override;
    void showEvent(QShowEvent *event) override;

private:
    void resizeWindow();

    QTimer mResizeTimer;
    QWindow *mWindow{};
    QWidget *mWindowContainer{};
};
