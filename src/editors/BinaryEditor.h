#ifndef BINARYEDITOR_H
#define BINARYEDITOR_H

#include "IEditor.h"
#include <QTableView>

class BinaryEditor : public QTableView, public IEditor
{
    Q_OBJECT
public:
    enum class DataType
    {
        Int8, Int16, Int32, Int64,
        Uint8, Uint16, Uint32, Uint64,
        Float, Double,
    };

    static bool load(const QString &fileName, QByteArray *data);
    static int getTypeSize(DataType type);

    explicit BinaryEditor(QString fileName, QWidget *parent = nullptr);
    ~BinaryEditor() override;

    QList<QMetaObject::Connection> connectEditActions(
        const EditActions &actions) override;
    QString fileName() const override { return mFileName; }
    void setFileName(QString fileName) override;
    bool load() override;
    bool reload() override;
    bool save() override;
    int tabifyGroup() override { return 0; }
    bool isModified() const { return mModified; }
    void replace(QByteArray data, bool invalidateFileCache = true);
    const QByteArray &data() const { return mData; }

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
    void updateColumns();
    void scrollToOffset();

signals:
    void modificationChanged(bool modified);
    void fileNameChanged(const QString &fileName);

private:
    class SpinBoxDelegate;
    class EditableRegion;
    class EditableRegionDelegate;
    class DataModel;
    class HexModel;

    struct Column
    {
        QString name;
        DataType type;
        int arity;
        int padding;
    };

    void handleDataChanged();
    void setModified(bool modified);
    Column *getColumn(int index);
    Column getColumn(int index) const;
    void refresh();

    QString mFileName;
    bool mModified{ };
    QByteArray mData;
    EditableRegion *mEditableRegion{ };
    int mRowHeight{ 20 };
    int mColumnWidth{ 32 };

    bool mColumnsInvalidated{ true };
    QList<Column> mColumns;
    int mOffset{ };
    int mStride{ };
    int mRowCount{ };
    int mPrevFirstRow{ };
};

#endif // BINARYEDITOR_H
