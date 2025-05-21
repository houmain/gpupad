#pragma once

#include "InputState.h"
#include <QJsonValue>
#include <QObject>

class MouseScriptObject final : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QJsonValue coord READ coord NOTIFY changed)
    Q_PROPERTY(QJsonValue fragCoord READ fragCoord NOTIFY changed)
    Q_PROPERTY(QJsonValue prevCoord READ prevCoord NOTIFY changed)
    Q_PROPERTY(QJsonValue prevFragCoord READ prevFragCoord NOTIFY changed)
    Q_PROPERTY(QJsonValue delta READ delta NOTIFY changed)
    Q_PROPERTY(QJsonValue buttons READ buttons NOTIFY changed)

public:
    explicit MouseScriptObject(QObject *parent = nullptr);

    void update(const InputState &state);

    QJsonValue coord() const;
    QJsonValue fragCoord() const;
    QJsonValue prevCoord() const;
    QJsonValue prevFragCoord() const;
    QJsonValue delta() const;
    QJsonValue buttons() const;
    bool wasRead() const { return mWasRead; }

Q_SIGNALS:
    void changed();

private:
    QJsonValue toCoord(QPoint coord) const;
    QJsonValue toFragCoord(QPoint coord) const;

    QSize mEditorSize{};
    QPoint mPosition{};
    QPoint mPrevPosition{};
    QVector<ButtonState> mButtons;
    mutable bool mWasRead{};
};
