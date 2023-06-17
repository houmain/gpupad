#include "FileBrowserWindow.h"
#include "Singletons.h"
#include "FileDialog.h"
#include "TitleBar.h"
#include <QTreeView>
#include <QFileSystemModel>
#include <QFileIconProvider>
#include <QBoxLayout>
#include <QComboBox>
#include <QToolButton>
#include <QStyle>

FileBrowserWindow::FileBrowserWindow(QWidget *parent) : QFrame(parent)
    , mModel(new QFileSystemModel(this))
    , mFileSystemTree(new QTreeView(this))
    , mRootDirectory(new QComboBox(this))
    , mBrowseButton(new QToolButton(this))
{
    setFrameShape(QFrame::Box);

    auto layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);
    layout->addWidget(mFileSystemTree);

    mBrowseButton->setIcon(QIcon(QIcon::fromTheme(
        QString::fromUtf8("document-open"))));
    mBrowseButton->setAutoRaise(true);

    mRootDirectory->setMinimumWidth(100);

    auto header = new QWidget(this);
    auto headerLayout = new QHBoxLayout(header);
    headerLayout->setContentsMargins(4, 4, 4, 4);
    headerLayout->setSpacing(2);
    headerLayout->addWidget(mRootDirectory);
    headerLayout->addWidget(mBrowseButton);

    auto titleBar = new TitleBar();
    titleBar->setWidget(header);
    mTitleBar = titleBar;

    mModel->setIconProvider(new QFileIconProvider());
    
    mFileSystemTree->setModel(mModel);
    mFileSystemTree->setDragEnabled(true);
    mFileSystemTree->setFrameShape(QFrame::NoFrame);
    mFileSystemTree->setHeaderHidden(true);
    mFileSystemTree->setColumnHidden(1, true);
    mFileSystemTree->setColumnHidden(2, true);
    mFileSystemTree->setColumnHidden(3, true);

    connect(mBrowseButton, &QToolButton::clicked, 
        this, &FileBrowserWindow::browseDirectory);
    connect(mFileSystemTree, &QTreeView::activated,
        this, &FileBrowserWindow::itemActivated);
    connect(&Singletons::fileDialog(), &FileDialog::directoryChanged,
        this, &FileBrowserWindow::currentDirectoryChanged);
    setRootPath(Singletons::fileDialog().directory().path());
}

void FileBrowserWindow::setRootPath(const QString &path)
{
    auto index = mModel->setRootPath(path);
    if (index != mFileSystemTree->rootIndex()) {
        mFileSystemTree->setRootIndex(index);
        mFileSystemTree->collapseAll();

        // TODO:
        mRootDirectory->clear();
        mRootDirectory->addItem(path);
    }
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

void FileBrowserWindow::browseDirectory()
{
    auto options = FileDialog::Options{ };
    options.setFlag(FileDialog::Directory);
    if (Singletons::fileDialog().exec(options)) {
        setRootPath(Singletons::fileDialog().fileName());
    }
}
