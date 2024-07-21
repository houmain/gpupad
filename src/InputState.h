#pragma once

#include <QObject>
#include <QPoint>
#include <QSize>
#include <QVector>
#include <vector>

enum class ButtonState {
    Up = 0,
    Down = 1,
    Pressed = 2,
    Released = -1,
};

class InputState : public QObject
{
    Q_OBJECT
public:
    InputState();

    void setEditorSize(QSize size);
    void setMousePosition(const QPoint &position);
    void setMouseButtonPressed(Qt::MouseButton button);
    void setMouseButtonReleased(Qt::MouseButton button);
    void setKeyPressed(int key, bool isAutoRepeat);
    void setKeyReleased(int key);
    void update();

    const QSize &editorSize() const { return mEditorSize; }
    const QPoint &mousePosition() const { return mMousePosition; }
    const QPoint &prevMousePosition() const { return mPrevMousePosition; }
    const QVector<ButtonState> &mouseButtonStates() const
    {
        return mMouseButtonStates;
    }
    const QVector<ButtonState> &keyStates() const { return mKeyStates; }

Q_SIGNALS:
    void mouseChanged();
    void keysChanged();

private:
    using ButtonStateQueue = std::vector<std::pair<int, ButtonState>>;

    QSize mNextEditorSize{};
    QPoint mNextMousePosition{};
    ButtonStateQueue mNextMouseButtonStates;
    ButtonStateQueue mNextKeyStates;

    QSize mEditorSize{};
    QPoint mMousePosition{};
    QPoint mPrevMousePosition{};
    QVector<ButtonState> mMouseButtonStates;
    QVector<ButtonState> mKeyStates;
};
