#pragma once

#include <QPoint>
#include <QSize>

struct InputState
{
    QSize editorSize;
    QPoint prevMousePosition;
    QPoint mousePosition;
    Qt::MouseButtons mouseButtonsPressed;
};
