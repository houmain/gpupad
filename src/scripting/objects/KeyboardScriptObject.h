#pragma once

#include "InputState.h"
#include <QJsonValue>
#include <QObject>

class KeyboardScriptObject final : public QObject
{
    Q_OBJECT
    // Qt key -> state; States: 0 up, 1 down, 2 pressed, -1 released.
    // keysByKeyCode is a dense array of 256 legacy browser codes.
    Q_PROPERTY(QJsonValue keys READ keys NOTIFY changed)
    Q_PROPERTY(QJsonValue keysByKeyCode READ keysByKeyCode NOTIFY changed)

public:
    explicit KeyboardScriptObject(QObject *parent = nullptr);

    void update(const InputState &state);

    const QJsonValue &keys() const;
    const QJsonValue &keysByKeyCode() const;
    bool wasRead() const { return mWasRead; }

Q_SIGNALS:
    void changed();

private:
    QJsonValue mKeys;
    mutable QJsonValue mKeysByKeyCode;
    mutable bool mWasRead{ };
};
