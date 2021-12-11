#pragma once

#include <QObject>
#include <QVector>
#include <QSize>
#include <vector>

enum class ButtonState
{
    Up       =  0,
    Down     =  1,
    Pressed  =  2,
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
    void update();

    const QSize &editorSize() const { return mEditorSize; }    
    const QPoint &mousePosition() const { return mMousePosition; }
    const QPoint &prevMousePosition() const { return mPrevMousePosition; }
    const QVector<ButtonState> &mouseButtonStates() const { return mMouseButtonStates; }

Q_SIGNALS:
    void changed();

private:
    QSize mNextEditorSize{ };
    QPoint mNextMousePosition{ };
    std::vector<std::pair<Qt::MouseButton, ButtonState>> mNextMouseButtonStates;

    QSize mEditorSize{ };
    QPoint mMousePosition{ };
    QPoint mPrevMousePosition{ };
    QVector<ButtonState> mMouseButtonStates;
};
