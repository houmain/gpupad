#pragma once

#include <QMouseEvent>

inline QPoint getMousePosition(const QMouseEvent *event)
{
#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
    return event->pos();
#else
    return event->position().toPoint();
#endif
}

inline QPoint getGlobalMousePosition(const QMouseEvent *event)
{
#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
    return event->globalPos();
#else
    return event->globalPosition().toPoint();
#endif
}
