
#include "InputState.h"
#include <QSet>

namespace {
    int getMouseButtonIndex(Qt::MouseButton button)
    {
        switch (button) {
        case Qt::LeftButton:    return 0;
        case Qt::MiddleButton:  return 1;
        case Qt::RightButton:   return 2;
        case Qt::BackButton:    return 3;
        case Qt::ForwardButton: return 4;
        default:                return 5;
        }
    }
} // namespace

InputState::InputState()
{
    mMouseButtonStates.resize(5);
    mKeyStates.resize(128);
}

void InputState::update()
{
    mEditorSize = mNextEditorSize;
    mPrevMousePosition = mMousePosition;
    mMousePosition = mNextMousePosition;

    const auto updateButtonStates = [](ButtonStateQueue &nextButtonStates,
                                        QVector<ButtonState> &buttonStates) {
        // only apply up to one update per button at once
        auto buttonsUpdated = QSet<int>();
        for (auto it = nextButtonStates.begin();
             it != nextButtonStates.end();) {
            const auto [buttonIndex, state] = *it;
            if (!buttonsUpdated.contains(buttonIndex)) {
                if (buttonIndex < buttonStates.size())
                    buttonStates[buttonIndex] = state;
                buttonsUpdated.insert(buttonIndex);
                it = nextButtonStates.erase(it);
            } else {
                ++it;
            }
        }

        // when it was not updated, convert from Pressed to Down...
        for (auto i = 0; i < buttonStates.size(); ++i)
            if (!buttonsUpdated.contains(i)) {
                if (buttonStates[i] == ButtonState::Pressed)
                    buttonStates[i] = ButtonState::Down;
                else if (buttonStates[i] == ButtonState::Released)
                    buttonStates[i] = ButtonState::Up;
            }
    };
    updateButtonStates(mNextMouseButtonStates, mMouseButtonStates);
    updateButtonStates(mNextKeyStates, mKeyStates);
}

void InputState::setEditorSize(QSize size)
{
    if (mNextEditorSize != size) {
        mNextEditorSize = size;
        Q_EMIT mouseChanged();
    }
}

void InputState::setMousePosition(const QPoint &position)
{
    if (mNextMousePosition != position) {
        mNextMousePosition = position;
        Q_EMIT mouseChanged();
    }
}

void InputState::setMouseButtonPressed(Qt::MouseButton button)
{
    mNextMouseButtonStates.emplace_back(getMouseButtonIndex(button),
        ButtonState::Pressed);
    Q_EMIT mouseChanged();
}

void InputState::setMouseButtonReleased(Qt::MouseButton button)
{
    mNextMouseButtonStates.emplace_back(getMouseButtonIndex(button),
        ButtonState::Released);
    Q_EMIT mouseChanged();
}

void InputState::setKeyPressed(int key, bool isAutoRepeat)
{
    if (!isAutoRepeat)
        mNextKeyStates.emplace_back(key, ButtonState::Pressed);
    Q_EMIT keysChanged();
}

void InputState::setKeyReleased(int key)
{
    mNextKeyStates.emplace_back(key, ButtonState::Released);
    Q_EMIT keysChanged();
}
