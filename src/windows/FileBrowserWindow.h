#pragma once

#include <QFrame>

class QFileSystemModel;
class QTreeView;
class QDir;
class QModelIndex;
class QComboBox;
class QToolButton;
class QStringListModel;

class FileBrowserWindow final : public QFrame
{
    Q_OBJECT

public:
    explicit FileBrowserWindow(QWidget *parent = nullptr);
    QWidget *titleBar() { return mTitleBar; }

    void setRootPath(const QString &path);
    void browseDirectory();
    void showInFileManager();

Q_SIGNALS:
    void fileActivated(const QString &filename);
    void previewFileChanged(const QString &filename);

private:
    void itemActivated(const QModelIndex &index);
    void currentDirectoryChanged(const QDir &dir);
    void selectionChanged(const QModelIndex &current,
        const QModelIndex &previous);
    bool revealDirectory(const QDir &dir);
    void focusDirectory(const QDir &dir);
    void updateRecentDirectories(const QString &path);
    QString completeToRecentDirectory(const QString &path);

    QWidget *mTitleBar{};
    QFileSystemModel *mModel{};
    QTreeView *mFileSystemTree{};
    QComboBox *mRootDirectory{};
    QToolButton *mBrowseButton{};
    QToolButton *mShowInFileManagerButton{};
    QToolButton *mPreviewFileButton{};
    QStringListModel *mRecentDirectories{};
};
