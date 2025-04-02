#pragma once

#include "editors/IEditor.h"
#include <QTableView>

class BinaryEditorToolBar;

class BinaryEditor final : public QTableView, public IEditor
{
    Q_OBJECT
public:
    enum class DataType {
        Int8,
        Int16,
        Int32,
        Int64,
        Uint8,
        Uint16,
        Uint32,
        Uint64,
        Float,
        Double,
    };

    struct Field
    {
        QString name;
        DataType dataType;
        int count;
        int padding;
    };

    struct Block
    {
        QString name;
        int offset;
        int rowCount;
        QList<Field> fields;
    };

    static int getTypeSize(DataType type);
    static int getStride(const Block &block);

    BinaryEditor(QString fileName, BinaryEditorToolBar *editorToolbar,
        QWidget *parent = nullptr);
    ~BinaryEditor() override;

    QList<QMetaObject::Connection> connectEditActions(
        const EditActions &actions) override;
    QString fileName() const override { return mFileName; }
    void setFileName(QString fileName) override;
    bool load() override;
    bool save() override;
    int tabifyGroup() const override { return 0; }
    void setModified() override;
    bool isModified() const { return mModified; }
    void replace(QByteArray data, bool emitFileChanged = true);
    void replaceRange(int offset, QByteArray data, bool emitFileChanged = true);
    const QByteArray &data() const { return mData; }
    void setBlocks(QList<Block> blocks);
    const QList<Block> &blocks() const { return mBlocks; }
    void setCurrentBlockIndex(int index);
    void scrollToOffset();

Q_SIGNALS:
    void modificationChanged(bool modified);
    void fileNameChanged(const QString &fileName);

protected:
    void wheelEvent(QWheelEvent *event) override;

private:
    class SpinBoxDelegate;
    class EditableRegion;
    class EditableRegionDelegate;
    class DataModel;
    class HexModel;

    void handleDataChanged();
    void setModified(bool modified);
    const Block *currentBlock() const;
    void refresh();
    void updateEditorToolBar();

    BinaryEditorToolBar &mEditorToolBar;
    QString mFileName;
    bool mModified{};
    QByteArray mData;
    HexModel *mHexModel{};
    DataModel *mDataModel{};
    EditableRegion *mEditableRegion{};
    int mRowHeight{ 20 };
    int mColumnWidth{ 32 };
    QList<Block> mBlocks;
    int mCurrentBlockIndex{};
    int mPrevFirstRow{};
};

QString toHexString(uint8_t value);
QString toHexString(uint64_t value);
