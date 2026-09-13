#pragma once

#if defined(MULTIMEDIA_ENABLED)

#  include "MediaSource.h"
#  include <QObject>
#  include <QVideoFrame>
#  include <chrono>

class TextureData;

class MediaStream : public QObject
{
    Q_OBJECT
public:
    explicit MediaStream(MediaSource source, QObject *parent = nullptr);

    const MediaSource &source() const { return mSource; }
    const QString &fileName() const { return mSource.fileName; }
    int width() const { return mWidth; }
    int height() const { return mHeight; }
    bool isReady() const { return mReady; }
    virtual void seek(std::chrono::milliseconds targetTime) = 0;

Q_SIGNALS:
    void readyChanged();

protected:
    void presentFrame(const QVideoFrame &frame);
    void presentTexture(TextureData texture);
    void finishLoading();
    void setReady(bool ready);

private:
    void setSize(int width, int height);

    MediaSource mSource;
    int mWidth{ };
    int mHeight{ };
    QVideoFrame mCurrentFrame;
    bool mReady{ };
};

#endif // defined(MULTIMEDIA_ENABLED)
