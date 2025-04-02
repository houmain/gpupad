#pragma once

#include "BinaryEditor.h"
#include <QAbstractTableModel>

class BinaryEditor::HexModel final : public QAbstractTableModel
{
    Q_OBJECT
public:
    HexModel(QObject *parent = nullptr) : QAbstractTableModel(parent) { }

    void setData(const QByteArray *data, int offset, int stride, int rowCount)
    {
        const auto prevData = mData;
        const auto prevOffset = mOffset;
        const auto prevStride = mStride;
        const auto prevRowCount = mRowCount;

        mData = data;
        mOffset = offset;
        mStride = stride;

        const auto lastRow = (mOffset + mStride * rowCount + mStride - 1)
            / mStride;
        rowCount = mData->size() / mStride + 2;
        while (getOffset(rowCount - 1, 0) >= mData->size())
            --rowCount;
        mRowCount = std::max(rowCount, lastRow);

        if (prevData == mData && prevOffset == mOffset && prevStride == mStride
            && prevRowCount == mRowCount)
            return;

        beginResetModel();
        endResetModel();
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
        if (role != Qt::DisplayRole && role != Qt::EditRole
            && role != Qt::TextAlignmentRole)
            return QVariant();

        if (!index.isValid())
            return QVariant();

        if (role == Qt::TextAlignmentRole) {
            if (index.column() == 0)
                return static_cast<int>(Qt::AlignRight | Qt::AlignVCenter);
            if (index.row() == (mOffset + mStride - 1) / mStride)
                return static_cast<int>(Qt::AlignHCenter | Qt::AlignBottom);
            return static_cast<int>(Qt::AlignHCenter | Qt::AlignVCenter);
        }

        const auto offset = getOffset(index.row(), index.column());
        if (offset < 0 || offset >= mData->size())
            return QVariant();

        return toHexString(static_cast<uint8_t>(mData->constData()[offset]));
    }

    QVariant headerData(int section, Qt::Orientation orientation,
        int role) const override
    {
        if (role == Qt::DisplayRole) {
            if (orientation == Qt::Horizontal)
                return toHexString(static_cast<uint8_t>(section));

            const auto offset = getOffset(section, 0);
            if (offset >= 0)
                return toHexString(static_cast<uint64_t>(offset));
        }
        return {};
    }

    Qt::ItemFlags flags(const QModelIndex &) const override { return {}; }

private:
    const QByteArray *mData{};
    int mOffset{};
    int mStride{};
    int mRowCount{};
};
