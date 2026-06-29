
#include "WindowWidget.h"
#include <QApplication>

WindowWidget::WindowWidget(bool forwardInputEvents, QWidget *parent)
    : QWidget(parent)
    , mForwardInputEvents(forwardInputEvents)
{
    mResizeTimer.setSingleShot(true);
    connect(&mResizeTimer, &QTimer::timeout, this, &WindowWidget::resizeWindow);
}

void WindowWidget::setWidgetWindow(QWindow *window)
{
    if (mWindow == window)
        return;
    if (mWindowContainer) {
        if (mForwardInputEvents)
            mWindow->removeEventFilter(this);
        mWindowContainer->deleteLater();
        mWindowContainer = nullptr;
        hide();
    }
    mWindow = window;
    if (window)
        show();
}

void WindowWidget::showEvent(QShowEvent *event)
{
    QWidget::showEvent(event);

    if (mWindow && !mWindowContainer) {
        mWindowContainer = QWidget::createWindowContainer(mWindow, this);
        if (mForwardInputEvents) {
            mWindow->installEventFilter(this);
            parentWidget()->setMouseTracking(true);
        }
        scheduleResize();
    }
}

void WindowWidget::resizeEvent(QResizeEvent *event)
{
    QWidget::resizeEvent(event);
    scheduleResize();
}

void WindowWidget::scheduleResize()
{
    // reduce buffer resizes on Windows, which glitches heavily
#if defined(_WIN32)
    const auto delay = 20;
#else
    const auto delay = 1;
#endif
    mResizeTimer.start(delay);
}

void WindowWidget::resizeWindow()
{
    if (!mWindowContainer)
        return;

    // show after the widget has its final size
    mWindowContainer->setGeometry(rect());
    mWindowContainer->show();
    mWindow->requestUpdate();
}

bool WindowWidget::eventFilter(QObject *watched, QEvent *event)
{
    if (mInEventFilter)
        return QWidget::eventFilter(watched, event);;
    mInEventFilter = true;
    const auto guard = qScopeGuard([&]() { mInEventFilter = false; });

    switch (event->type()) {
    case QEvent::Enter:
    case QEvent::Leave:
    case QEvent::Wheel:
    case QEvent::MouseButtonDblClick:
    case QEvent::MouseButtonPress:
    case QEvent::MouseMove:
    case QEvent::MouseButtonRelease:
    case QEvent::KeyPress:
    case QEvent::KeyRelease:
    case QEvent::DragEnter:
    case QEvent::DragMove:
    case QEvent::DragLeave:
    case QEvent::Drop:
        if (QWidget *p = parentWidget())
            QApplication::sendEvent(p, event);
        return true;
    default: break;
    }
    return QWidget::eventFilter(watched, event);
}
