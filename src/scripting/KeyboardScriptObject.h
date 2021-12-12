#pragma once

#include <QObject>
#include <QJsonValue>
#include "InputState.h"

class KeyboardScriptObject final : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QJsonValue keys READ keys)

public:
    explicit KeyboardScriptObject(QObject *parent = nullptr);

    void update(const InputState &state);

    const QJsonValue &keys() const;
    bool wasRead() const { return mWasRead; }

private:
    QJsonValue mKeys;
    mutable bool mWasRead{ };
};
