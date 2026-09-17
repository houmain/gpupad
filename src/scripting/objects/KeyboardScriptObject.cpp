#include "KeyboardScriptObject.h"
#include <QJsonArray>
#include <QJsonObject>

namespace {
    int getKeyCode(Qt::Key key)
    {
        // Legacy browser key codes
        switch (key) {
        case Qt::Key_Backspace: return 8;
        case Qt::Key_Tab:       return 9;
        case Qt::Key_Return:
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
        default:                return -1;
        }
    }
} // namespace

KeyboardScriptObject::KeyboardScriptObject(QObject *parent) : QObject(parent)
{
    update(InputState());
}

void KeyboardScriptObject::update(const InputState &state)
{
    mWasRead = false;
    auto keys = QJsonObject();
    for (auto it = state.keyStates().cbegin(); it != state.keyStates().cend();
        ++it) {
        const auto value = static_cast<int>(it.value());
        keys.insert(QString::number(it.key()), value);
    }
    mKeys = keys;
    mKeysByKeyCode = QJsonValue();
    Q_EMIT changed();
}

const QJsonValue &KeyboardScriptObject::keys() const
{
    mWasRead = true;
    return mKeys;
}

const QJsonValue &KeyboardScriptObject::keysByKeyCode() const
{
    mWasRead = true;
    if (!mKeysByKeyCode.isNull())
        return mKeysByKeyCode;

    auto keysByKeyCode = QJsonArray();
    for (auto i = 0; i < 256; ++i)
        keysByKeyCode.append(0);
    const auto keys = mKeys.toObject();
    for (auto it = keys.begin(); it != keys.end(); ++it) {
        const auto value = it.value().toInt();
        const auto keyCode = getKeyCode(static_cast<Qt::Key>(it.key().toInt()));
        if (keyCode >= 0) {
            // Return and keypad Enter share a code. A held key takes priority
            // over the release of its alias, and a new press takes priority.
            const auto previous = keysByKeyCode[keyCode].toInt();
            if (previous == 0 || value > previous)
                keysByKeyCode[keyCode] = value;
        }
    }
    mKeysByKeyCode = keysByKeyCode;
    return mKeysByKeyCode;
}
