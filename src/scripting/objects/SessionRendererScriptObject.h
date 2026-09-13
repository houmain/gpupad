#pragma once

#include "media/MediaFrame.h"
#include <QObject>
#include <QVariantMap>
#include <memory>

class SessionRendererScriptObject final : public QObject
{
    Q_OBJECT

public:
    explicit SessionRendererScriptObject(const QVariantMap &options,
        QObject *parent = nullptr);
    ~SessionRendererScriptObject() override;

    Q_INVOKABLE void requestFrame(const QVariantMap &request);
    Q_INVOKABLE void close();

Q_SIGNALS:
    void frameReady(MediaFrame frame);

private:
    class State;
    std::unique_ptr<State> mState;
};
