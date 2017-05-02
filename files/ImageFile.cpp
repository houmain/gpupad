#include "ImageFile.h"
#include <cstring>
#include <QImage>

ImageFile::ImageFile(QObject *parent)
    : QObject(parent)
    , mImage(new QImage(1, 1, QImage::Format_ARGB32))
{
}

ImageFile::~ImageFile()
{
    if (isModified())
        emit dataChanged();
}

void ImageFile::setFileName(const QString &fileName)
{
    mFileName = fileName;
    emit fileNameChanged(fileName);
}

bool ImageFile::load()
{
    if (!mImage->load(fileName()))
        return false;
    setModified(false);
    return true;
}

bool ImageFile::save()
{
    if (!mImage->save(fileName()))
        return false;
    setModified(false);
    return true;
}

const QImage &ImageFile::image() const
{
    return *mImage;
}

void ImageFile::replace(QImage image)
{
    if (image == *mImage)
        return;

    *mImage = image;
    setModified(true);
    emit dataChanged();
}

void ImageFile::setModified(bool modified)
{
    if (mModified != modified) {
        mModified = modified;
        emit modificationChanged(modified);
    }
}
