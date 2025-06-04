#include "MouseScriptObject.h"
#include <QJsonArray>

namespace {
    std::atomic<int> gLastEditorWidth{ 2 };
    std::atomic<int> gLastEditorHeight{ 2 };
    std::atomic<int> gLastMousePositionX{ 1 };
    std::atomic<int> gLastMousePositionY{ 1 };
} // namespace

MouseScriptObject::MouseScriptObject(QObject *parent) : QObject(parent)
{
    auto inputState = InputState();
    inputState.setEditorSize({
        gLastEditorWidth.load(),
        gLastEditorHeight.load(),
    });
    inputState.setMousePosition({
        gLastMousePositionX.load(),
        gLastMousePositionY.load(),
    });
    inputState.update();
    inputState.update();
    update(inputState);
}

void MouseScriptObject::update(const InputState &state)
{
    gLastEditorWidth.store(state.editorSize().width());
    gLastEditorHeight.store(state.editorSize().height());
    gLastMousePositionX.store(state.mousePosition().x());
    gLastMousePositionY.store(state.mousePosition().y());

    mWasRead = false;
    mEditorSize = state.editorSize();
    mPosition = state.mousePosition();
    mPrevPosition = state.prevMousePosition();
    mButtons = state.mouseButtonStates();
    Q_EMIT changed();
}

QJsonValue MouseScriptObject::editorSize() const
{
    auto vector = QJsonArray();
    vector.append(mEditorSize.width());
    vector.append(mEditorSize.height());
    return vector;
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

QJsonValue MouseScriptObject::delta() const
{
    mWasRead = true;
    const auto delta = QPointF(mPosition - mPrevPosition);
    auto vector = QJsonArray();
    vector.append(delta.x() / mEditorSize.width());
    vector.append(delta.y() / mEditorSize.height());
    return vector;
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
