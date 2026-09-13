#pragma once

#if defined(MULTIMEDIA_ENABLED)

#  include "MediaStream.h"
#  include <QJsonArray>

class QMediaCaptureSession;
class QCamera;
class QVideoSink;

QJsonArray enumerateCameras();

class Camera final : public MediaStream
{
public:
    Camera(MediaSource source, QObject *parent = nullptr);

    void seek(std::chrono::milliseconds time) override;

private:
    void handleFrameDecoded(QVideoFrame frame);

    QMediaCaptureSession *mCaptureSession{ };
    QCamera *mCamera{ };
    QVideoSink *mSink{ };
    QVideoFrame mNextFrame;
    bool mAdvancing{ };
};

#endif // !MULTIMEDIA_ENABLED
