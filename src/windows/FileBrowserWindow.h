#pragma once

#include <QWidget>

class QFileSystemModel;
class QTreeView;
class QDir;
class QModelIndex;

class FileBrowserWindow final : public QWidget
{
    Q_OBJECT

public:
    explicit FileBrowserWindow(QWidget *parent = nullptr);

    void setRootPath(const QString &path);
    void setRootDirectory(const QDir &dir);

private Q_SLOTS:
    void itemActivated(const QModelIndex &index);

Q_SIGNALS:
    void fileActivated(const QString &filename);

private:
    QFileSystemModel *mModel{ };
    QTreeView *mFileSystemTree{ };
};
