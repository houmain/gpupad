#pragma once

#if defined(MULTIMEDIA_ENABLED)

#  include <QObject>
#  include <QVideoFrame>
#  include <chrono>

class TextureData;

class VideoStream : public QObject
{
    Q_OBJECT
public:
    explicit VideoStream(QString fileName, bool flipVertically,
        QObject *parent = nullptr);
    const QString &fileName() const { return mFileName; }
    int width() const { return mWidth; }
    int height() const { return mHeight; }
    virtual void seek(std::chrono::milliseconds targetTime) = 0;

Q_SIGNALS:
    void loadingFinished();

protected:
    void presentFrame(const QVideoFrame &frame);
    void presentTexture(TextureData texture);

private:
    void setSize(int width, int height);

    QString mFileName;
    bool mFlipVertically{ };
    int mWidth{ };
    int mHeight{ };
    QVideoFrame mCurrentFrame;
};

#endif // defined(MULTIMEDIA_ENABLED)
