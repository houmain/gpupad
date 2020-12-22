#ifndef BINARYEDITOR_H
#define BINARYEDITOR_H

#include "IEditor.h"
#include "ui_BinaryEditorToolBar.h"
#include <QTableView>

class BinaryEditor final : public QTableView, public IEditor
{
    Q_OBJECT
public:
    enum class DataType
    {
        Int8, Int16, Int32, Int64,
        Uint8, Uint16, Uint32, Uint64,
        Float, Double,
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
    static Ui::BinaryEditorToolBar *createEditorToolBar(QWidget *container);

    BinaryEditor(QString fileName,
        const Ui::BinaryEditorToolBar* editorToolbar, QWidget *parent = nullptr);
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
    void replace(QByteArray data, bool emitFileChanged = true);
    const QByteArray &data() const { return mData; }
    void setBlocks(QList<Block> blocks);
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

    const Ui::BinaryEditorToolBar &mEditorToolBar;
    QString mFileName;
    bool mModified{ };
    QByteArray mData;
    EditableRegion *mEditableRegion{ };
    int mRowHeight{ 20 };
    int mColumnWidth{ 32 };
    QList<Block> mBlocks;
    int mCurrentBlockIndex{ };
    int mPrevFirstRow{ };
};

#endif // BINARYEDITOR_H
