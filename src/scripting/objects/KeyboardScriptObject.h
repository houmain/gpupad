#pragma once

#include "InputState.h"
#include <QJsonValue>
#include <QObject>

class KeyboardScriptObject final : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QJsonValue keys READ keys NOTIFY changed)

public:
    explicit KeyboardScriptObject(QObject *parent = nullptr);

    void update(const InputState &state);

    const QJsonValue &keys() const;
    bool wasRead() const { return mWasRead; }

Q_SIGNALS:
    void changed();

private:
    QJsonValue mKeys;
    mutable bool mWasRead{};
};
