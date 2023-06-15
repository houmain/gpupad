#pragma once

#include <QFrame>

class QFileSystemModel;
class QTreeView;
class QDir;
class QModelIndex;

class FileBrowserWindow final : public QFrame
{
    Q_OBJECT

public:
    explicit FileBrowserWindow(QWidget *parent = nullptr);

    void setRootPath(const QString &path);

private Q_SLOTS:
    void itemActivated(const QModelIndex &index);
    void currentDirectoryChanged(const QDir &dir);

Q_SIGNALS:
    void fileActivated(const QString &filename);

private:
    QFileSystemModel *mModel{ };
    QTreeView *mFileSystemTree{ };
};
