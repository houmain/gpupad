#include "FileBrowserWindow.h"
#include "Singletons.h"
#include "FileDialog.h"
#include <QTreeView>
#include <QFileSystemModel>
#include <QBoxLayout>

FileBrowserWindow::FileBrowserWindow(QWidget *parent) : QFrame(parent)
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
    mFileSystemTree->setFrameShape(QFrame::NoFrame);

    connect(mFileSystemTree, &QTreeView::activated,
        this, &FileBrowserWindow::itemActivated);
    connect(&Singletons::fileDialog(), &FileDialog::directoryChanged,
        this, &FileBrowserWindow::currentDirectoryChanged);
    setRootPath(Singletons::fileDialog().directory().path());
}

void FileBrowserWindow::setRootPath(const QString &path)
{
    mFileSystemTree->setRootIndex(mModel->setRootPath(path));
}

void FileBrowserWindow::currentDirectoryChanged(const QDir &dir)
{
    const auto path = dir.path();
    const auto index = mModel->index(path);
    mFileSystemTree->setExpanded(index, true);
    if (!mFileSystemTree->contentsRect().intersects(mFileSystemTree->visualRect(index))) {
        mFileSystemTree->scrollTo(index, QTreeView::PositionAtTop);
        if (mFileSystemTree->visualRect(index).isEmpty())
            setRootPath(dir.path());
    }
}

void FileBrowserWindow::itemActivated(const QModelIndex &index)
{
    Q_EMIT fileActivated(mModel->fileInfo(index).filePath());
}
