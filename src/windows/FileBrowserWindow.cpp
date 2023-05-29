#include "FileBrowserWindow.h"
#include "Singletons.h"
#include "FileDialog.h"
#include <QTreeView>
#include <QFileSystemModel>
#include <QBoxLayout>

FileBrowserWindow::FileBrowserWindow(QWidget *parent) : QWidget(parent)
    , mModel(new QFileSystemModel(this))
    , mFileSystemTree(new QTreeView(this))
{
    auto layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->addWidget(mFileSystemTree);
    
    mFileSystemTree->setModel(mModel);    
    mFileSystemTree->setHeaderHidden(true);
    mFileSystemTree->setColumnHidden(1, true);
    mFileSystemTree->setColumnHidden(2, true);
    mFileSystemTree->setColumnHidden(3, true);

    mFileSystemTree->setDragEnabled(true);

    connect(&Singletons::fileDialog(), &FileDialog::directoryChanged,
        this, &FileBrowserWindow::setRootDirectory);
    setRootDirectory(Singletons::fileDialog().directory());

    connect(mFileSystemTree, &QTreeView::activated,
        this, &FileBrowserWindow::itemActivated);
}

void FileBrowserWindow::setRootPath(const QString &path)
{
    mFileSystemTree->setRootIndex(mModel->setRootPath(path));
}

void FileBrowserWindow::setRootDirectory(const QDir &dir)
{
    setRootPath(dir.path());
}

void FileBrowserWindow::itemActivated(const QModelIndex &index)
{
    Q_EMIT fileActivated(mModel->fileInfo(index).filePath());
}
