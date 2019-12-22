#include "InputScriptObject.h"

InputScriptObject::InputScriptObject(QObject *parent) : QObject(parent)
{
}

void InputScriptObject::setMousePosition(QPointF pos) 
{
    mMousePosition = pos; 
}

QJsonValue InputScriptObject::mouseX() const 
{
    return mMousePosition.x();
}

QJsonValue InputScriptObject::mouseY() const 
{
    return mMousePosition.y();
}
