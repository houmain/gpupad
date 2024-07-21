#pragma once

#include "BinaryEditor.h"
#include <QAbstractTableModel>

namespace {
    template <typename T>
    QString toHexString(T value, int digits)
    {
        return QString::number(value, 16).toUpper().rightJustified(digits, '0');
    }
} // namespace

class BinaryEditor::HexModel final : public QAbstractTableModel
{
    Q_OBJECT
public:
    HexModel(const QByteArray *data, int offset, int stride, int rowCount,
        QObject *parent)
        : QAbstractTableModel(parent)
        , mData(*data)
        , mOffset(offset)
        , mStride(stride)
    {
        const auto lastRow = (mOffset + mStride * rowCount + mStride - 1)
            / mStride;

        rowCount = mData.size() / mStride + 2;
        while (getOffset(rowCount - 1, 0) >= mData.size())
            --rowCount;

        mRowCount = std::max(rowCount, lastRow);
    }

    int rowCount(const QModelIndex &) const override { return mRowCount; }

    int columnCount(const QModelIndex &) const override { return mStride; }

    int getOffset(int row, int column) const
    {
        return mStride * row + column
            - (mStride - (mOffset % mStride)) % mStride;
    }

    QVariant data(const QModelIndex &index, int role) const override
    {
        if (!index.isValid()
            || (role != Qt::DisplayRole && role != Qt::EditRole
                && role != Qt::TextAlignmentRole))
            return QVariant();

        if (role == Qt::TextAlignmentRole) {
            if (index.column() == 0)
                return static_cast<int>(Qt::AlignRight | Qt::AlignVCenter);
            if (index.row() == (mOffset + mStride - 1) / mStride)
                return static_cast<int>(Qt::AlignHCenter | Qt::AlignBottom);
            return static_cast<int>(Qt::AlignHCenter | Qt::AlignVCenter);
        }

        if (role == Qt::DisplayRole || role == Qt::EditRole) {
            auto offset = getOffset(index.row(), index.column());
            if (offset < 0 || offset >= mData.size())
                return QVariant();

            return toHexString(static_cast<uint8_t>(mData.constData()[offset]),
                2);
        }
        return QVariant();
    }

    QVariant headerData(int section, Qt::Orientation orientation,
        int role) const override
    {
        if (role == Qt::DisplayRole) {
            if (orientation == Qt::Horizontal)
                return toHexString(section, 2);

            auto offset = getOffset(section, 0);
            if (offset >= 0)
                return toHexString(offset, 4);
        }
        return {};
    }

    Qt::ItemFlags flags(const QModelIndex &) const override { return {}; }

private:
    const QByteArray &mData;
    int mOffset{};
    int mStride{};
    int mRowCount{};
};
