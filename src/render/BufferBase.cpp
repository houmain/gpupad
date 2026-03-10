
#include "BufferBase.h"
#include "FileDialog.h"

BufferBase::BufferBase(int size) : mSize(size)
{
    Q_ASSERT(size > 0);
}

BufferBase::BufferBase(const Buffer &buffer, int size)
    : mItemId(buffer.id)
    , mFileName(buffer.fileName)
    , mSize(size)
{
    Q_ASSERT(size > 0);
}

void BufferBase::updateUntitledFilename(const BufferBase &rhs)
{
    if (mSize == rhs.mSize && FileDialog::isEmptyOrUntitled(mFileName)
        && FileDialog::isEmptyOrUntitled(rhs.mFileName))
        mFileName = rhs.mFileName;
}

bool BufferBase::operator==(const BufferBase &rhs) const
{
    return std::tie(mFileName, mSize, mMessages)
        == std::tie(rhs.mFileName, rhs.mSize, rhs.mMessages);
}

QByteArray &BufferBase::writableData()
{
    mSystemCopyModified = true;
    if (mData.isNull())
        mData.resize(mSize);
    return mData;
}

bool BufferBase::swap(BufferBase &other)
{
    if (mSize != other.mSize)
        return false;

    mDeviceCopyModified = true;
    other.mDeviceCopyModified = true;
    return true;
}