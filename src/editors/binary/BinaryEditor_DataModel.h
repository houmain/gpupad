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
public:
    DataModel(BinaryEditor *editor, const BinaryEditor::Block &block,
        QByteArray *data)
        : QAbstractTableModel(editor)
        , mEditor(*editor)
        , mOffset(block.offset)
        , mStride(getStride(block))
        , mRowCount(block.rowCount)
        , mData(*data)
    {
        auto index = 0;
        auto offset = 0;
        for (const auto &field : block.fields) {
            if (field.count) {
                mColumns.append({ index, offset, field.dataType, field.count,
                    field.name, true });

                index += field.count;
                offset += getTypeSize(field.dataType) * field.count;
            }
            // fill padding with not editable column sets
            if (field.padding) {
                mColumns.append({ index, offset, DataType::Uint8, field.padding,
                    "", false });

                index += field.padding;
                offset += field.padding;
            }
        }
        mColumnCount = index;
    }

    int getColumnSize(int index) const
    {
        if (auto column = getColumn(index))
            return getTypeSize(column->type);
        return 0;
    }

    int rowCount(const QModelIndex &) const override { return mRowCount; }

    int columnCount(const QModelIndex &) const override { return mColumnCount; }

    QVariant data(const QModelIndex &index, int role) const override
    {
        if (!index.isValid()
            || (role != Qt::DisplayRole && role != Qt::EditRole
                && role != Qt::TextAlignmentRole && role != Qt::UserRole))
            return QVariant();

        auto column = getColumn(index.column());
        if (!column)
            return QVariant();

        if (role == Qt::UserRole)
            return static_cast<int>(column->type);

        if (role == Qt::TextAlignmentRole) {
            if (column->editable)
                return static_cast<int>(Qt::AlignRight | Qt::AlignVCenter);
            return static_cast<int>(Qt::AlignHCenter | Qt::AlignVCenter);
        }

        if (role == Qt::DisplayRole || role == Qt::EditRole) {
            auto columnOffset = getColumnOffset(index.column());
            auto offset = mOffset + mStride * index.row() + columnOffset;
            auto columnSize = getTypeSize(column->type);
            if (columnOffset + columnSize > mStride
                || offset + columnSize > mData.size())
                return QVariant();

            if (!column->editable)
                return toHexString(getByte(offset), 2);

            return getData(offset, column->type);
        }
        return QVariant();
    }

    bool setData(const QModelIndex &index, const QVariant &variant,
        int role) override
    {
        if (role == Qt::EditRole) {
            const auto column = getColumn(index.column());
            if (!column)
                return false;

            const auto columnOffset = getColumnOffset(index.column());
            const auto columnSize = getTypeSize(column->type);
            if (columnOffset + columnSize > mStride)
                return false;

            const auto offset = mOffset + mStride * index.row();
            expand(offset + mStride);

            setData(offset + columnOffset, column->type, variant);
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
            auto column = getColumn(section);
            if (column && column->editable) {
                const auto index = section - column->index;
                return QString(column->name
                    + (column->count > 1 ? QString("[%1]").arg(index) : ""));
            }
        }

        return QVariant();
    }

    Qt::ItemFlags flags(const QModelIndex &index) const override
    {
        if (auto set = getColumn(index.column()))
            if (set->editable)
                return Qt::ItemIsEditable | Qt::ItemIsSelectable
                    | Qt::ItemIsEnabled;
        return {};
    }

private:
    struct Column
    {
        int index;
        int offset;
        DataType type;
        int count;
        QString name;
        bool editable;
    };

    const Column *getColumn(int index) const
    {
        for (const auto &c : mColumns)
            if (index < c.index + c.count)
                return &c;
        return nullptr;
    }

    int getColumnOffset(int index) const
    {
        if (auto column = getColumn(index))
            return column->offset
                + getTypeSize(column->type) * (index - column->index);
        return 0;
    }

    uint8_t getByte(int offset) const
    {
        return static_cast<uint8_t>(mData.constData()[offset]);
    }

    QVariant getData(int offset, DataType type) const
    {
        auto data = mData.constData() + offset;
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
        auto data = mData.data() + offset;
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
        if (requiredSize > mData.size()) {
            mData.append(QByteArray(requiredSize - mData.size(), 0));
        }
    }

    BinaryEditor &mEditor;
    const int mOffset;
    const int mStride;
    const int mRowCount;
    QByteArray &mData;
    QList<Column> mColumns;
    int mColumnCount{};
};
