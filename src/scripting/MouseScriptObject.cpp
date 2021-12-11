#include "MouseScriptObject.h"
#include <QJsonArray>

MouseScriptObject::MouseScriptObject(QObject *parent) : QObject(parent)
{
}

void MouseScriptObject::update(const InputState &state)
{
    mWasRead = false;
    mEditorSize = state.editorSize();
    mPosition = state.mousePosition();
    mPrevPosition = state.prevMousePosition();
    mButtons = state.mouseButtonStates();
}

QJsonValue MouseScriptObject::coord() const
{
    mWasRead = true;
    return toCoord(mPosition);
}

QJsonValue MouseScriptObject::fragCoord() const
{
    mWasRead = true;
    return toFragCoord(mPosition);
}

QJsonValue MouseScriptObject::prevCoord() const
{
    mWasRead = true;
    return toCoord(mPrevPosition);
}

QJsonValue MouseScriptObject::prevFragCoord() const
{
    mWasRead = true;
    return toFragCoord(mPrevPosition);
}

QJsonValue MouseScriptObject::buttons() const 
{
    mWasRead = true;
    auto array = QJsonArray();
    for (auto button : mButtons)
        array.append(static_cast<int>(button));
    return array;
}

QJsonValue MouseScriptObject::toCoord(QPoint coord) const
{
    auto vector = QJsonArray();
    vector.append((coord.x() + 0.5) / mEditorSize.width() * 2 - 1);
    vector.append((coord.y() + 0.5) / mEditorSize.height() * 2 - 1);
    return vector;
}

QJsonValue MouseScriptObject::toFragCoord(QPoint coord) const
{
    auto vector = QJsonArray();
    vector.append(coord.x() + 0.5);
    vector.append(coord.y() + 0.5);
    return vector;
}
