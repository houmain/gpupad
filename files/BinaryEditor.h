#ifndef BINARYEDITOR_H
#define BINARYEDITOR_H

#include "FileTypes.h"
#include "BinaryFile.h"
#include <QTableView>

class BinaryEditor : public QTableView
{
    Q_OBJECT
public:
    using DataType = BinaryFile::DataType;

    explicit BinaryEditor(BinaryFilePtr file, QWidget *parent = 0);

    QList<QMetaObject::Connection> connectEditActions(const EditActions &actions);
    const BinaryFilePtr &file() const { return mFile; }
    void refresh();
    void setOffset(int offset);
    int offset() const { return mOffset; }
    void setStride(int stride = 0);
    int stride() const { return mStride; }
    void setRowCount(int rowCount);
    int rowCount() const { return mRowCount; }
    void setColumnCount(int count);
    int columnCount() const { return mColumns.size(); }
    void setColumnName(int index, QString name);
    QString columnName(int index) const { return getColumn(index).name; }
    void setColumnType(int index, DataType type);
    DataType columnType(int index) const { return getColumn(index).type; }
    void setColumnArity(int index, int arity);
    int columnArity(int index) const { return getColumn(index).arity; }
    void setColumnPadding(int index, int padding);
    int columnPadding(int index) const { return getColumn(index).padding; }

private:
    class DataEditor;

    struct Column
    {
        QString name;
        DataType type;
        int arity;
        int padding;
    };

    Column *getColumn(int index)
    {
        return (index < mColumns.size() ? &mColumns[index] : nullptr);
    }

    Column getColumn(int index) const
    {
        return (index < mColumns.size() ? mColumns[index] : Column{ });
    }

    template <typename T>
    void update(T& property, const T& value)
    {
        mInvalidated |= (property != value);
        property = value;
    }

    bool mInvalidated{ true };
    BinaryFilePtr mFile;
    DataEditor *mDataEditor;
    int mRowHeight{ 20 };
    int mColumnWidth{ 32 };
    QList<Column> mColumns;
    int mOffset{ };
    int mStride{ 16 };
    int mRowCount{ };
    int mPrevFirstRow{ };
};

#endif // BINARYEDITOR_H
