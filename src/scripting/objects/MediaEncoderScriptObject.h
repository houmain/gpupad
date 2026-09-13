#pragma once

#include "media/MediaFrame.h"
#include <QJsonObject>
#include <QObject>
#include <QVariantMap>
#include <memory>

class MediaEncoderScriptObject final : public QObject
{
    Q_OBJECT

public:
    explicit MediaEncoderScriptObject(const QVariantMap &options,
        QObject *parent = nullptr);
    ~MediaEncoderScriptObject() override;

    static QJsonObject configurations();

    Q_INVOKABLE void writeFrame(MediaFrame frame);
    Q_INVOKABLE void close();

Q_SIGNALS:
    void frameWritten();
    void finished(QString errorString);

private:
    class State;
    std::unique_ptr<State> mState;
};
