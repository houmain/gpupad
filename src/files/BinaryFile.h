#ifndef BINARYFILE_H
#define BINARYFILE_H

#include <QObject>
#include <QVariant>

class QByteArray;

class BinaryFile : public QObject
{
    Q_OBJECT
public:
    enum class DataType
    {
        Int8, Int16, Int32, Int64,
        Uint8, Uint16, Uint32, Uint64,
        Float, Double,
    };

    explicit BinaryFile(QObject *parent = 0);
    ~BinaryFile();

    bool isUndoAvailable() const;
    void undo();
    bool isRedoAvailable() const;
    void redo();
    bool isModified() const { return mModified; }
    void setFileName(const QString &fileName);
    const QString &fileName() const { return mFileName; }
    bool load();
    bool save();

    void replace(QByteArray data);
    int size() const;
    const QByteArray &data() const;
    uint8_t getByte(int offset) const;
    QVariant getData(int offset, DataType type) const;
    void setData(int offset, DataType type, QVariant value);
    void expand(int requiredSize);

signals:
    void dataChanged();
    void modificationChanged(bool modified);
    void fileNameChanged(const QString &fileName);

private:
    void setModified(bool modified);

    QString mFileName;
    QScopedPointer<QByteArray> mData;
    bool mModified{ };
};

#endif // BINARYFILE_H
