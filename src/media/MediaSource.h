#pragma once

#include "session/Item.h"
#include <QMetaType>
#include <QSize>
#include <QString>

inline constexpr auto ChannelCount = 2;
inline constexpr auto BytesPerFrame = int{ ChannelCount * sizeof(float) };

struct MediaSource
{
    QString fileName;
    Texture::SourceType type{ Texture::SourceType::NoSource };
    Texture::Target target{ Texture::Target::Target2D };
    int width{ };
    int height{ };

    MediaSource() = default;
    MediaSource(QString fileName) : fileName(fileName) { }
    MediaSource(QString fileName, Texture::SourceType type,
        Texture::Target target = Texture::Target::Target2D,
        QSize resolution = { })
        : fileName(fileName)
        , type(type)
        , target(target)
        , width(std::max(resolution.width(), 1))
        , height(1)
    {
    }

    QSize resolution() const { return QSize(width, height); }

    std::strong_ordering operator<=>(const MediaSource &) const = default;
};

Q_DECLARE_METATYPE(MediaSource)
