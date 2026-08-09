
#include "Camera.h"

#if defined(MULTIMEDIA_ENABLED)

#  include <QMediaDevices>
#  include <QMediaCaptureSession>
#  include <QCamera>
#  include <QVideoSink>
#  include <QJsonObject>
#  include <QFile>
#  include <QFileInfo>

namespace {
    std::pair<QString, QVideoFrameFormat> parseCameraJson(QString fileName)
    {
        if (QFile file(fileName); file.open(QIODevice::ReadOnly))
            if (const auto json = QJsonDocument::fromJson(file.readAll());
                json.isObject()) {

                const auto camera = json.object();
                const auto id = camera.value("id").toString();
                const auto resolutionX = camera.value("resolutionX").toInt();
                const auto resolutionY = camera.value("resolutionY").toInt();
                const auto pixelFormat = [](QString name) {
                    for (auto i = 0; i < QVideoFrameFormat::NPixelFormats; ++i)
                        if (auto f =
                                static_cast<QVideoFrameFormat::PixelFormat>(i);
                            QVideoFrameFormat::pixelFormatToString(f) == name)
                            return f;
                    return QVideoFrameFormat::Format_Invalid;
                }(camera.value("pixelFormat").toString());

                return {
                    id,
                    QVideoFrameFormat{
                        QSize{ resolutionX, resolutionY },
                        pixelFormat,
                    },
                };
            }
        return { };
    }

    QCameraDevice selectCameraDevice(const QString &id, const QString &name)
    {
        const auto videoInputs = QMediaDevices::videoInputs();
        if (videoInputs.isEmpty())
            return { };

        for (const auto &videoInput : videoInputs)
            if (videoInput.id() == id)
                return videoInput;

        for (const auto &videoInput : videoInputs)
            if (videoInput.description() == name)
                return videoInput;

        for (const auto &videoInput : videoInputs)
            if (videoInput.isDefault())
                return videoInput;

        return videoInputs.front();
    }

    QCameraFormat selectDeviceFormat(const QCameraDevice &device,
        const QVideoFrameFormat &frameFormat)
    {
        for (const auto &format : device.videoFormats()) {
            if (frameFormat.pixelFormat() != QVideoFrameFormat::Format_Invalid
                && frameFormat.pixelFormat() != format.pixelFormat())
                continue;

            if (!frameFormat.frameSize().isEmpty()
                && frameFormat.frameSize() != format.resolution())
                continue;

            return format;
        }
        return { };
    }
} // namespace

QJsonArray enumerateCameras()
{
    auto cameras = QJsonArray();
    const auto videoInputs = QMediaDevices::videoInputs();
    for (const auto &videoInput : videoInputs) {
        const auto videoFormats = videoInput.videoFormats();
        if (videoFormats.isEmpty())
            continue;
        auto camera = QJsonObject();
        camera.insert("name", videoInput.description());
        camera.insert("id", QString(videoInput.id()));
        if (videoInput.isDefault())
            camera.insert("isDefault", true);
        auto formats = QJsonArray();
        for (const auto &inputFormat : videoFormats) {
            auto format = QJsonObject();
            format.insert("pixelFormat",
                QVideoFrameFormat::pixelFormatToString(
                    inputFormat.pixelFormat()));
            format.insert("resolutionX", inputFormat.resolution().width());
            format.insert("resolutionY", inputFormat.resolution().height());
            format.insert("minFrameRate", inputFormat.minFrameRate());
            format.insert("maxFrameRate", inputFormat.maxFrameRate());
            formats.append(format);
        }
        camera.insert("formats", formats);
        cameras.append(camera);
    }
    return cameras;
}

Camera::Camera(QString fileName, bool flipVertically, QObject *parent)
    : VideoStream(fileName, flipVertically, parent)
{
    const auto [id, frameFormat] = parseCameraJson(fileName);
    const auto name = QFileInfo(fileName).baseName();
    const auto device = selectCameraDevice(id, name);
    if (device.isNull()) {
        loadingFinished();
        return;
    }

    mCaptureSession = new QMediaCaptureSession(this);
    mCamera = new QCamera(device, this);
    connect(mCamera, &QCamera::errorOccurred, this,
        [this](QCamera::Error error) {
            if (error != QCamera::NoError && !width())
                loadingFinished();
        });

    const auto format = selectDeviceFormat(device, frameFormat);
    if (!format.isNull())
        mCamera->setCameraFormat(format);

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
