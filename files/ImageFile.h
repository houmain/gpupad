#ifndef IMAGEFILE_H
#define IMAGEFILE_H

#include <QObject>
#include "FileTypes.h"

class QImage;

class ImageFile : public QObject
{
    Q_OBJECT
public:
    ImageFile(QObject *parent = 0);
    ~ImageFile();

    bool isUndoAvailable() const;
    void undo();
    bool isRedoAvailable() const;
    void redo();
    bool isModified() const { return mModified; }
    void setFileName(const QString &fileName);
    const QString &fileName() const { return mFileName; }
    bool load();
    bool save();

    const QImage &image() const;
    void replace(QImage image);

signals:
    void dataChanged();
    void modificationChanged(bool modified);
    void fileNameChanged(const QString &fileName);

private:
    void setModified(bool modified);

    QString mFileName;
    QScopedPointer<QImage> mImage;
    bool mModified{ };
};

#endif // IMAGEFILE_H
