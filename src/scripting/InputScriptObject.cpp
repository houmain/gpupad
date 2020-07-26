#include "InputScriptObject.h"
#include <QJsonArray>

InputScriptObject::InputScriptObject(QObject *parent) : QObject(parent)
{
}

void InputScriptObject::setMouseFragCoord(QPointF coord)
{
    mMouseFragCoord = coord;
}

QJsonValue InputScriptObject::mouseFragCoord() const
{
    auto array = QJsonArray();
    array.append(mMouseFragCoord.x());
    array.append(mMouseFragCoord.y());
    return array;
}
