#include "BinaryFile.h"
#include <QByteArray>
#include <QFile>

namespace {
    template <typename T> const T &cast(const void* ptr)
    { return *static_cast<const T*>(ptr); }

    template <typename T> T &cast(void* ptr)
    { return *static_cast<T*>(ptr); }
} // namespace

BinaryFile::BinaryFile(QObject *parent)
    : QObject(parent)
    , mData(new QByteArray())
{
}

BinaryFile::~BinaryFile()
{
    if (isModified())
        emit dataChanged();
}

void BinaryFile::setFileName(const QString &fileName)
{
    mFileName = fileName;
    emit fileNameChanged(fileName);
}

bool BinaryFile::load()
{
    QFile file(fileName());
    if (file.open(QFile::ReadOnly)) {
        *mData = file.readAll();
        setModified(false);
        return true;
    }
    return false;
}

bool BinaryFile::save()
{
    QFile file(fileName());
    if (file.open(QFile::WriteOnly | QFile::Truncate)) {
        file.write(*mData);
        setModified(false);
        return true;
    }
    return false;
}

void BinaryFile::replace(QByteArray data)
{
    if (data == *mData)
        return;

    *mData = data;
    setModified(true);
    emit dataChanged();
}

int BinaryFile::size() const
{
    return mData->size();
}

const QByteArray &BinaryFile::data() const
{
    return *mData;
}

uint8_t BinaryFile::getByte(int offset) const
{
    return static_cast<uint8_t>(mData->data()[offset]);
}

QVariant BinaryFile::getData(int offset, DataType type) const
{
    auto data = mData->data() + offset;
    switch (type) {
        case DataType::Int8: return cast<int8_t>(data);
        case DataType::Int16: return cast<int16_t>(data);
        case DataType::Int32: return cast<int32_t>(data);
        case DataType::Int64: return cast<qlonglong>(data);
        case DataType::Uint8: return cast<uint8_t>(data);
        case DataType::Uint16: return cast<uint16_t>(data);
        case DataType::Uint32: return cast<uint32_t>(data);
        case DataType::Uint64: return cast<qulonglong>(data);
        case DataType::Float: return cast<float>(data);
        case DataType::Double: return cast<double>(data);
    }
    return { };
}

void BinaryFile::setData(int offset, DataType type, QVariant v)
{
    auto data = mData->data() + offset;
    switch (type) {
        case DataType::Int8: cast<int8_t>(data) = v.toInt(); break;
        case DataType::Int16: cast<int16_t>(data) = v.toInt(); break;
        case DataType::Int32: cast<int32_t>(data) = v.toInt(); break;
        case DataType::Int64: cast<int64_t>(data) = v.toLongLong(); break;
        case DataType::Uint8: cast<uint8_t>(data) = v.toUInt(); break;
        case DataType::Uint16: cast<uint16_t>(data) = v.toUInt(); break;
        case DataType::Uint32: cast<uint32_t>(data) = v.toUInt(); break;
        case DataType::Uint64: cast<uint64_t>(data) = v.toULongLong(); break;
        case DataType::Float: cast<float>(data) = v.toFloat(); break;
        case DataType::Double: cast<double>(data) = v.toDouble(); break;
    }
    setModified(true);
    emit dataChanged();
}

void BinaryFile::expand(int requiredSize)
{
    if (requiredSize > size()) {
        mData->append(QByteArray(requiredSize - size(), 0));
        setModified(true);
        emit dataChanged();
    }
}

void BinaryFile::setModified(bool modified)
{
    if (mModified != modified) {
        mModified = modified;
        emit modificationChanged(modified);
    }
}
