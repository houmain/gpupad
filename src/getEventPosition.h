#pragma once

#include <QEvent>

template <typename Event>
QPoint getEventPosition(const Event *event)
{
#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
    return event->pos();
#else
    return event->position().toPoint();
#endif
}

template <typename Event>
QPoint getGlobalEventPosition(const Event *event)
{
#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
    return event->globalPos();
#else
    return event->globalPosition().toPoint();
#endif
}
