
#include "Camera.h"

#if defined(MULTIMEDIA_ENABLED)

#  include <QMediaDevices>
#  include <QMediaCaptureSession>
#  include <QCamera>
#  include <QVideoSink>

Camera::Camera(QString fileName, bool flipVertically, QObject *parent)
    : VideoStream(fileName, flipVertically, parent)
{
    // TODO:
    const auto devices = QMediaDevices::videoInputs();
    auto it = devices.begin();

    mCaptureSession = new QMediaCaptureSession(this);
    mCamera = new QCamera(*it, this);
    connect(mCamera, &QCamera::errorOccurred, this,
        [this](QCamera::Error error) {
            if (error != QCamera::NoError && !width())
                loadingFinished();
        });
    mSink = new QVideoSink(this);
    connect(mSink, &QVideoSink::videoFrameChanged, this,
        &Camera::handleFrameDecoded);
    mCaptureSession->setCamera(mCamera);
    mCaptureSession->setVideoSink(mSink);
    mCamera->start();
}

void Camera::handleFrameDecoded(QVideoFrame frame)
{
    // immediately present when advancing, otherwise present on next seek
    mNextFrame = frame;
    if (!width() || mAdvancing)
        presentFrame(std::exchange(mNextFrame, { }));
    mAdvancing = false;
}

void Camera::seek(std::chrono::milliseconds time)
{
    if (mNextFrame.isValid())
        presentFrame(std::exchange(mNextFrame, { }));
    mAdvancing = true;
}

#endif // defined(MULTIMEDIA_ENABLED)
