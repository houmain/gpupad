#pragma once

#include "BinaryEditor.h"
#include <QAbstractTableModel>

namespace {
    template <typename T>
    T get(const void *ptr)
    {
        return *static_cast<const T *>(ptr);
    }

    template <typename T, typename S>
    void set(void *ptr, const S &value)
    {
        *static_cast<T *>(ptr) = static_cast<T>(value);
    }
} // namespace

class BinaryEditor::DataModel final : public QAbstractTableModel
{
    Q_OBJECT
public:
    DataModel(BinaryEditor *editor)
        : QAbstractTableModel(editor)
        , mEditor(*editor)
    {
    }

    void setData(QByteArray *data, const BinaryEditor::Block &block)
    {
        const auto prevData = mData;
        const auto prevOffset = mOffset;
        const auto prevStride = mStride;
        const auto prevRowCount = mRowCount;

        mData = data;
        mOffset = block.offset;
        mStride = getStride(block);
        mRowCount = block.rowCount;

        if (prevData == mData && prevOffset == mOffset && prevStride == mStride
            && prevRowCount == mRowCount)
            return;

        auto offset = 0;
        mColumns.clear();
        for (const auto &field : block.fields) {
            for (auto i = 0; i < field.count; ++i) {
                mColumns.append({ offset, field.dataType, field.name, true });
                if (field.count > 1) {
                    auto &name = mColumns.back().name;
                    name = QStringLiteral("%1[%2]").arg(name).arg(i);
                }
                offset += getTypeSize(field.dataType);
            }
            // fill padding with not editable column sets
            if (field.padding) {
                for (auto i = 0; i < field.padding; ++i) {
                    mColumns.append({ offset, DataType::Uint8, "", false });
                    ++offset;
                }
            }
        }

        beginResetModel();
        endResetModel();
    }

    int getColumnSize(int index) const
    {
        return getTypeSize(getColumn(index).type);
    }

    int rowCount(const QModelIndex &) const override { return mRowCount; }

    int columnCount(const QModelIndex &) const override
    {
        return mColumns.size();
    }

    QVariant data(const QModelIndex &index, int role) const override
    {
        if (role != Qt::DisplayRole && role != Qt::EditRole
            && role != Qt::TextAlignmentRole && role != Qt::UserRole)
            return QVariant();

        if (!index.isValid())
            return QVariant();

        const auto &column = getColumn(index.column());
        if (role == Qt::UserRole)
            return static_cast<int>(column.type);

        if (role == Qt::TextAlignmentRole) {
            if (column.editable)
                return static_cast<int>(Qt::AlignRight | Qt::AlignVCenter);
            return static_cast<int>(Qt::AlignHCenter | Qt::AlignVCenter);
        }

        const auto offset = mOffset + mStride * index.row() + column.offset;
        const auto columnSize = getTypeSize(column.type);
        if (column.offset + columnSize > mStride
            || offset + columnSize > mData->size())
            return QVariant();

        if (!column.editable)
            return toHexString(getByte(offset));

        return getData(offset, column.type);
    }

    bool setData(const QModelIndex &index, const QVariant &variant,
        int role) override
    {
        if (role == Qt::EditRole) {
            const auto &column = getColumn(index.column());
            const auto columnSize = getTypeSize(column.type);
            if (column.offset + columnSize > mStride)
                return false;

            const auto offset = mOffset + mStride * index.row();
            expand(offset + mStride);

            setData(offset + column.offset, column.type, variant);
        }
        return true;
    }

    QVariant headerData(int section, Qt::Orientation orientation,
        int role) const override
    {
        if (role == Qt::TextAlignmentRole)
            return static_cast<int>(Qt::AlignLeft | Qt::AlignVCenter);

        if (role == Qt::DisplayRole && orientation == Qt::Vertical)
            return section;

        if ((role == Qt::DisplayRole || role == Qt::ToolTipRole)
            && orientation == Qt::Horizontal) {
            const auto &column = getColumn(section);
            if (column.editable)
                return column.name;
        }

        return QVariant();
    }

    Qt::ItemFlags flags(const QModelIndex &index) const override
    {
        const auto &column = getColumn(index.column());
        if (column.editable)
            return Qt::ItemIsEditable | Qt::ItemIsSelectable
                | Qt::ItemIsEnabled;
        return {};
    }

private:
    struct Column
    {
        int offset;
        DataType type;
        QString name;
        bool editable;
    };

    const Column &getColumn(int index) const
    {
        Q_ASSERT(index < mColumns.size());
        return mColumns[index];
    }

    uint8_t getByte(int offset) const
    {
        return static_cast<uint8_t>(mData->constData()[offset]);
    }

    QVariant getData(int offset, DataType type) const
    {
        auto data = mData->constData() + offset;
        switch (type) {
        case DataType::Int8:   return get<int8_t>(data);
        case DataType::Int16:  return get<int16_t>(data);
        case DataType::Int32:  return get<int32_t>(data);
        case DataType::Int64:  return get<qlonglong>(data);
        case DataType::Uint8:  return get<uint8_t>(data);
        case DataType::Uint16: return get<uint16_t>(data);
        case DataType::Uint32: return get<uint32_t>(data);
        case DataType::Uint64: return get<qulonglong>(data);
        case DataType::Float:  return get<float>(data);
        case DataType::Double: return get<double>(data);
        }
        return {};
    }

    void setData(int offset, DataType type, QVariant v)
    {
        auto data = mData->data() + offset;
        switch (type) {
        case DataType::Int8:   set<int8_t>(data, v.toInt()); break;
        case DataType::Int16:  set<int16_t>(data, v.toInt()); break;
        case DataType::Int32:  set<int32_t>(data, v.toInt()); break;
        case DataType::Int64:  set<int64_t>(data, v.toLongLong()); break;
        case DataType::Uint8:  set<uint8_t>(data, v.toUInt()); break;
        case DataType::Uint16: set<uint16_t>(data, v.toUInt()); break;
        case DataType::Uint32: set<uint32_t>(data, v.toUInt()); break;
        case DataType::Uint64: set<uint64_t>(data, v.toULongLong()); break;
        case DataType::Float:  set<float>(data, v.toFloat()); break;
        case DataType::Double: set<double>(data, v.toDouble()); break;
        }
    }

    void expand(int requiredSize)
    {
        if (requiredSize > mData->size()) {
            mData->append(QByteArray(requiredSize - mData->size(), 0));
        }
    }

    BinaryEditor &mEditor;
    QByteArray *mData{};
    int mOffset{};
    int mStride{};
    int mRowCount{};
    QList<Column> mColumns;
};
