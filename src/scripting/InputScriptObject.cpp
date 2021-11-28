#include "InputScriptObject.h"
#include <QJsonArray>

InputScriptObject::InputScriptObject(QObject *parent) : QObject(parent)
{
}

void InputScriptObject::update(const InputState &inputState)
{
    mWasRead = false;
    mInputState = inputState;
}

QJsonValue InputScriptObject::mouseFragCoord() const
{
    mWasRead = true;
    auto array = QJsonArray();
    array.append(mInputState.mousePosition.x() + 0.5);
    array.append(mInputState.mousePosition.y() + 0.5);
    return array;
}

QJsonValue InputScriptObject::mouseCoord() const
{
    mWasRead = true;
    auto array = QJsonArray();
    array.append((mInputState.mousePosition.x() + 0.5) / 
        mInputState.editorSize.width() * 2 - 1);
    array.append((mInputState.mousePosition.y() + 0.5) / 
        mInputState.editorSize.height() * 2 - 1);
    return array;
}

QJsonValue InputScriptObject::prevMouseFragCoord() const
{
    mWasRead = true;
    auto array = QJsonArray();
    array.append(mInputState.prevMousePosition.x() + 0.5);
    array.append(mInputState.prevMousePosition.y() + 0.5);
    return array;
}

QJsonValue InputScriptObject::prevMouseCoord() const
{
    mWasRead = true;
    auto array = QJsonArray();
    array.append((mInputState.prevMousePosition.x() + 0.5) / 
        mInputState.editorSize.width() * 2 - 1);
    array.append((mInputState.prevMousePosition.y() + 0.5) / 
        mInputState.editorSize.height() * 2 - 1);
    return array;
}

QJsonValue InputScriptObject::mousePressed() const
{
    mWasRead = true;
    auto array = QJsonArray();
    array.append((mInputState.mouseButtonsPressed & Qt::LeftButton) != 0);
    array.append((mInputState.mouseButtonsPressed & Qt::MiddleButton) != 0);
    array.append((mInputState.mouseButtonsPressed & Qt::RightButton) != 0);
    return array;    
}
