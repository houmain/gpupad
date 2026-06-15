
#include "WindowWidget.h"

WindowWidget::WindowWidget(QWidget *parent) : QWidget(parent)
{
    mResizeTimer.setSingleShot(true);
    connect(&mResizeTimer, &QTimer::timeout, this, &WindowWidget::resizeWindow);
}

void WindowWidget::setWindow(QWindow *window)
{
    if (mWindow == window)
        return;
    if (mWindowContainer) {
        mWindowContainer->deleteLater();
        mWindowContainer = nullptr;
        hide();
    }
    if (mWindow)
        mWindow->deleteLater();
    mWindow = window;
    if (window)
        show();
}

void WindowWidget::showEvent(QShowEvent *event)
{
    QWidget::showEvent(event);

    if (mWindow && !mWindowContainer) {
        mWindowContainer = QWidget::createWindowContainer(mWindow, this);
        mWindowContainer->setGeometry(rect());
        mWindowContainer->show();
    }
    resizeWindow();
}

void WindowWidget::resizeEvent(QResizeEvent *event)
{
    QWidget::resizeEvent(event);
    mResizeTimer.start(50);
}

void WindowWidget::resizeWindow()
{
    if (!mWindowContainer || !mWindow)
        return;

    mWindowContainer->setGeometry(rect());
    mWindow->requestUpdate();
}
