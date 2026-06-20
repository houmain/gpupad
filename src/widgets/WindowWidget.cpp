
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
        delete mWindowContainer;
        mWindowContainer = nullptr;
        hide();
    }
    if (mWindow)
        delete mWindow;
    mWindow = window;
    if (window)
        show();
}

void WindowWidget::showEvent(QShowEvent *event)
{
    QWidget::showEvent(event);

    if (mWindow && !mWindowContainer)
        mWindowContainer = QWidget::createWindowContainer(mWindow, this);

    resizeWindow();
}

void WindowWidget::resizeEvent(QResizeEvent *event)
{
    QWidget::resizeEvent(event);
    if (mWindowContainer)
        mResized = true;

#if defined(_WIN32)
    // reduce buffer resizes on Windows, which glitches heavily
    mResizeTimer.start(20);
#else
    resizeWindow();
#endif
}

void WindowWidget::resizeWindow()
{
    if (!mWindowContainer)
        return;

    // show after the widget has its final size
    mWindowContainer->setGeometry(rect());
    if (mResized)
        mWindowContainer->show();
    mWindow->requestUpdate();
}
