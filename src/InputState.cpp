
#include "InputState.h"
#include <QDateTime>
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
    reset();
}

void InputState::update(EvaluationType evaluationType,
    std::optional<double> soundTime)
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
    // Keep Qt key values intact, including keys outside the legacy 0-255 range.
    auto keysUpdated = QSet<int>();
    for (auto it = mNextKeyStates.begin(); it != mNextKeyStates.end();) {
        const auto [key, state] = *it;
        if (!keysUpdated.contains(key)) {
            mKeyStates[key] = state;
            keysUpdated.insert(key);
            it = mNextKeyStates.erase(it);
        } else {
            ++it;
        }
    }
    for (auto it = mKeyStates.begin(); it != mKeyStates.end();) {
        if (!keysUpdated.contains(it.key())) {
            if (it.value() == ButtonState::Pressed)
                it.value() = ButtonState::Down;
            else if (it.value() == ButtonState::Released)
                it.value() = ButtonState::Up;
        }
        if (it.value() == ButtonState::Up)
            it = mKeyStates.erase(it);
        else
            ++it;
    }

    const auto now = Clock::now();
    mFrameIndex += 1;
    switch (evaluationType) {
    case EvaluationType::Reset:
        mFrameIndex = 0;
        mTime = 0;
        break;

    case EvaluationType::Automatic:
        // do not advance time
        break;

    case EvaluationType::Manual: mTime += mManualTimeStep; break;

    case EvaluationType::Steady:
        if (soundTime.has_value()) {
            mTime = soundTime.value();
        } else if (mLastUpdateTime.time_since_epoch().count() > 0) {
            mTime +=
                std::chrono::duration<double>(now - mLastUpdateTime).count();
        }
        break;
    }

    // only measure elapsed time between two steady evaluations
    mLastUpdateTime =
        (evaluationType == EvaluationType::Steady ? now : Clock::time_point());

    Q_EMIT frameIndexChanged(mFrameIndex);
    Q_EMIT timeChanged(mTime);
}

void InputState::setFlipCoordY(bool flipCoordY)
{
    mFlipCoordY = flipCoordY;
}

void InputState::setFrameIndex(int frameIndex)
{
    if (std::exchange(mFrameIndex, frameIndex) != frameIndex)
        Q_EMIT frameIndexChanged(mFrameIndex);
}

void InputState::setTime(double time)
{
    if (std::exchange(mTime, time) != time) {
        mTimeSeeked = true;
        Q_EMIT timeChanged(mTime);
    }
}

bool InputState::resetTimeSeeked()
{
    return std::exchange(mTimeSeeked, false);
}

void InputState::reset()
{
    mTimeSeeked = false;
    mFrameIndex = 0;
    mTime = 0;
}

void InputState::restoreEditorSize(QSize size)
{
    mNextEditorSize = mEditorSize = size;
}

void InputState::setEditorSize(QSize size)
{
    if (std::exchange(mNextEditorSize, size) != size)
        Q_EMIT mouseChanged();
}

void InputState::restoreMousePosition(const QPoint &position)
{
    mNextMousePosition = mPrevMousePosition = mMousePosition = position;
}

void InputState::setMousePosition(const QPoint &position)
{
    if (std::exchange(mNextMousePosition, position) != position)
        Q_EMIT mouseChanged();
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
    mNextKeyStates.emplace_back(key, ButtonState::Pressed);
    Q_EMIT keysChanged();
}

void InputState::setKeyReleased(Qt::Key key)
{
    mNextKeyStates.emplace_back(key, ButtonState::Released);
    Q_EMIT keysChanged();
}
