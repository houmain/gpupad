
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

    int getKeyIndex(Qt::Key key)
    {
        // TODO: add more keys on demand
        switch (key) {
        case Qt::Key_Backspace: return 8;
        case Qt::Key_Tab:       return 9;
        case Qt::Key_Enter:     return 13;
        case Qt::Key_Shift:     return 16;
        case Qt::Key_Control:   return 17;
        case Qt::Key_Alt:       return 18;
        case Qt::Key_Escape:    return 27;
        case Qt::Key_Space:     return 32;
        case Qt::Key_Left:      return 37;
        case Qt::Key_Up:        return 38;
        case Qt::Key_Right:     return 39;
        case Qt::Key_Down:      return 40;
        case Qt::Key_0:         return 48;
        case Qt::Key_1:         return 49;
        case Qt::Key_2:         return 50;
        case Qt::Key_3:         return 51;
        case Qt::Key_4:         return 52;
        case Qt::Key_5:         return 53;
        case Qt::Key_6:         return 54;
        case Qt::Key_7:         return 55;
        case Qt::Key_8:         return 56;
        case Qt::Key_9:         return 57;
        case Qt::Key_A:         return 65;
        case Qt::Key_B:         return 66;
        case Qt::Key_C:         return 67;
        case Qt::Key_D:         return 68;
        case Qt::Key_E:         return 69;
        case Qt::Key_F:         return 70;
        case Qt::Key_G:         return 71;
        case Qt::Key_H:         return 72;
        case Qt::Key_I:         return 73;
        case Qt::Key_J:         return 74;
        case Qt::Key_K:         return 75;
        case Qt::Key_L:         return 76;
        case Qt::Key_M:         return 77;
        case Qt::Key_N:         return 78;
        case Qt::Key_O:         return 79;
        case Qt::Key_P:         return 80;
        case Qt::Key_Q:         return 81;
        case Qt::Key_R:         return 82;
        case Qt::Key_S:         return 83;
        case Qt::Key_T:         return 84;
        case Qt::Key_U:         return 85;
        case Qt::Key_V:         return 86;
        case Qt::Key_W:         return 87;
        case Qt::Key_X:         return 88;
        case Qt::Key_Y:         return 89;
        case Qt::Key_Z:         return 90;
        case Qt::Key_F1:        return 112;
        case Qt::Key_F2:        return 113;
        case Qt::Key_F3:        return 114;
        case Qt::Key_F4:        return 115;
        case Qt::Key_F5:        return 116;
        case Qt::Key_F6:        return 117;
        case Qt::Key_F7:        return 118;
        case Qt::Key_F8:        return 119;
        case Qt::Key_F9:        return 120;
        case Qt::Key_F10:       return 121;
        case Qt::Key_F11:       return 122;
        case Qt::Key_F12:       return 123;
        case Qt::Key_AltGr:     return 225;
        default:                return 0;
        }
    }
} // namespace

InputState::InputState()
{
    mMouseButtonStates.resize(5);
    mKeyStates.resize(256);
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

void InputState::setKeyPressed(Qt::Key key)
{
    mNextKeyStates.emplace_back(getKeyIndex(key), ButtonState::Pressed);
    Q_EMIT keysChanged();
}

void InputState::setKeyReleased(Qt::Key key)
{
    mNextKeyStates.emplace_back(getKeyIndex(key), ButtonState::Released);
    Q_EMIT keysChanged();
}
