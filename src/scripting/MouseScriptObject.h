#pragma once

#include <QObject>
#include <QJsonValue>
#include "InputState.h"

class MouseScriptObject final : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QJsonValue coord READ coord CONSTANT)
    Q_PROPERTY(QJsonValue fragCoord READ fragCoord CONSTANT)
    Q_PROPERTY(QJsonValue prevCoord READ prevCoord CONSTANT)
    Q_PROPERTY(QJsonValue prevFragCoord READ prevFragCoord CONSTANT)
    Q_PROPERTY(QJsonValue buttons READ buttons CONSTANT)

public:
    explicit MouseScriptObject(QObject *parent = nullptr);

    void update(const InputState &state);

    QJsonValue coord() const;
    QJsonValue fragCoord() const;
    QJsonValue prevCoord() const;
    QJsonValue prevFragCoord() const;
    QJsonValue buttons() const;
    bool wasRead() const { return mWasRead; }

private:
    QJsonValue toCoord(QPoint coord) const;
    QJsonValue toFragCoord(QPoint coord) const;

    QSize mEditorSize{ };
    QPoint mPosition{ };
    QPoint mPrevPosition{ };
    QVector<ButtonState> mButtons;
    mutable bool mWasRead{ };
};
