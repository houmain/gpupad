
#include "InputState.h"
#include <QDateTime>
#include <QSet>

InputState::InputState()
{
    reset();
}

void InputState::update(EvaluationType evaluationType,
    std::optional<double> soundTime)
{
    mEditorSize = mNextEditorSize;
    mPrevMousePosition = mMousePosition;
    mMousePosition = mNextMousePosition;

    const auto updateButtonStates = [](ButtonStateQueue &nextStates,
                                        QMap<int, ButtonState> &states) {
        // Preserve Qt values and apply at most one transition per button/frame.
        auto updated = QSet<int>();
        for (auto it = nextStates.begin(); it != nextStates.end();) {
            const auto [button, state] = *it;
            if (!updated.contains(button)) {
                states[button] = state;
                updated.insert(button);
                it = nextStates.erase(it);
            } else {
                ++it;
            }
        }
        for (auto it = states.begin(); it != states.end();) {
            if (!updated.contains(it.key())) {
                if (it.value() == ButtonState::Pressed)
                    it.value() = ButtonState::Down;
                else if (it.value() == ButtonState::Released)
                    it.value() = ButtonState::Up;
            }
            if (it.value() == ButtonState::Up)
                it = states.erase(it);
            else
                ++it;
        }
    };
    updateButtonStates(mNextMouseButtonStates, mMouseButtonStates);
    updateButtonStates(mNextKeyStates, mKeyStates);

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
    mNextMouseButtonStates.emplace_back(button,
        ButtonState::Pressed);
    Q_EMIT mouseChanged();
}

void InputState::setMouseButtonReleased(Qt::MouseButton button)
{
    mNextMouseButtonStates.emplace_back(button,
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
