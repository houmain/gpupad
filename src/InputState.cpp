
#include "InputState.h"
#include <QSet>

namespace
{
    int getMouseButtonIndex(Qt::MouseButton button)
    {
        switch (button) {
            case Qt::LeftButton: return 0;
            case Qt::MiddleButton: return 1;
            case Qt::RightButton: return 2;
            case Qt::BackButton: return 3;
            case Qt::ForwardButton: return 4;
            default: return 5;
        }
    }
} // namespace

InputState::InputState() 
{
    mMouseButtonStates.resize(5);
}

void InputState::update()
{
    mEditorSize = mNextEditorSize;
    mPrevMousePosition = mMousePosition;
    mMousePosition = mNextMousePosition;

    auto buttonsUpdated = QSet<int>();
    for (auto it = mNextMouseButtonStates.begin();
         it != mNextMouseButtonStates.end(); ) {
        const auto buttonIndex = getMouseButtonIndex(it->first);
        if (!buttonsUpdated.contains(buttonIndex)) {
            mMouseButtonStates[buttonIndex] = it->second;
            buttonsUpdated.insert(buttonIndex);
            it = mNextMouseButtonStates.erase(it);
        }
        else {
            ++it;
        }
    }

    for (auto i = 0; i < mMouseButtonStates.size(); ++i)
        if (!buttonsUpdated.contains(i)) {
            if (mMouseButtonStates[i] == ButtonState::Pressed)
                mMouseButtonStates[i] = ButtonState::Down;
            else if (mMouseButtonStates[i] == ButtonState::Released)
                mMouseButtonStates[i] = ButtonState::Up;
        }
}

void InputState::setEditorSize(QSize size)
{
    if (mNextEditorSize != size) {
        mNextEditorSize = size;
        Q_EMIT changed();
    }
}

void InputState::setMousePosition(const QPoint &position)
{
    if (mNextMousePosition != position) {
        mNextMousePosition = position;
        Q_EMIT changed();
    }
}

void InputState::setMouseButtonPressed(Qt::MouseButton button)
{
    mNextMouseButtonStates.emplace_back(button, ButtonState::Pressed);
    Q_EMIT changed();
}

void InputState::setMouseButtonReleased(Qt::MouseButton button)
{
    mNextMouseButtonStates.emplace_back(button, ButtonState::Released);
    Q_EMIT changed();
}
