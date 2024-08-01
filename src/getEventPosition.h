#pragma once

#include <QEvent>

template <typename Event>
QPoint getEventPosition(const Event *event)
{
    return event->position().toPoint();
}

template <typename Event>
QPoint getGlobalEventPosition(const Event *event)
{
    return event->globalPosition().toPoint();
}
