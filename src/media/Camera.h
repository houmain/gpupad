#pragma once

#if defined(MULTIMEDIA_ENABLED)

#  include "VideoStream.h"
#  include <QJsonArray>

class QMediaCaptureSession;
class QCamera;
class QVideoSink;

QJsonArray enumerateCameras();

class Camera final : public VideoStream
{
public:
    Camera(QString fileName, bool flipVertically, QObject *parent = nullptr);

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
