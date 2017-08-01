#ifndef FILECACHE_H
#define FILECACHE_H

#include <QObject>
#include <QMap>
#include <QImage>
#include <QMutex>

class EditorManager;

class FileCache : public QObject
{
    Q_OBJECT
public:
    explicit FileCache(QObject *parent = 0);

    bool getSource(const QString &fileName, QString *source) const;
    bool getImage(const QString &fileName, QImage *image) const;
    bool getBinary(const QString &fileName, QByteArray *binary) const;

    // only call from main thread
    void update(EditorManager &editorManager,
                const QSet<QString> &filesModified);

private:
    mutable QMutex mMutex;
    mutable QMap<QString, QString> mSources;
    mutable QMap<QString, QImage> mImages;
    mutable QMap<QString, QByteArray> mBinaries;
};

#endif // FILECACHE_H
