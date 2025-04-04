#include "FileBrowserWindow.h"
#include "FileDialog.h"
#include "Singletons.h"
#include "WindowTitle.h"
#include <QBoxLayout>
#include <QComboBox>
#include <QFileIconProvider>
#include <QFileSystemModel>
#include <QStringListModel>
#include <QStyle>
#include <QToolButton>
#include <QTreeView>

FileBrowserWindow::FileBrowserWindow(QWidget *parent)
    : QFrame(parent)
    , mModel(new QFileSystemModel(this))
    , mFileSystemTree(new QTreeView(this))
    , mRootDirectory(new QComboBox(this))
    , mBrowseButton(new QToolButton(this))
    , mShowInFileManagerButton(new QToolButton(this))
    , mRecentDirectories(new QStringListModel(this))
{
    setFrameShape(QFrame::Box);

    auto layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);
    layout->addWidget(mFileSystemTree);

    mBrowseButton->setIcon(
        QIcon(QIcon::fromTheme(QString::fromUtf8("document-open"))));
    mBrowseButton->setAutoRaise(true);

    mShowInFileManagerButton->setIcon(
        QIcon(QIcon::fromTheme(QString::fromUtf8("dialog-information"))));
    mShowInFileManagerButton->setAutoRaise(true);

    for (const auto &dir : getApplicationDirectories(ActionsDir))
        updateRecentDirectories(dir.path());

    mRootDirectory->setMinimumWidth(100);
    mRootDirectory->setModel(mRecentDirectories);

    auto header = new QWidget(this);
    auto headerLayout = new QHBoxLayout(header);
    headerLayout->setContentsMargins(4, 4, 4, 4);
    headerLayout->setSpacing(2);
    headerLayout->addWidget(mRootDirectory);
    headerLayout->addWidget(mBrowseButton);
    headerLayout->addWidget(mShowInFileManagerButton);

    auto titleBar = new WindowTitle();
    titleBar->setWidget(header);
    mTitleBar = titleBar;

    mModel->setIconProvider(new QFileIconProvider());

    mFileSystemTree->setModel(mModel);
    mFileSystemTree->setSelectionMode(QTreeView::ExtendedSelection);
    mFileSystemTree->setSelectionBehavior(QTreeView::SelectRows);
    mFileSystemTree->setDragEnabled(true);
    mFileSystemTree->setFrameShape(QFrame::NoFrame);
    mFileSystemTree->setHeaderHidden(true);
    mFileSystemTree->setColumnHidden(1, true);
    mFileSystemTree->setColumnHidden(2, true);
    mFileSystemTree->setColumnHidden(3, true);

    connect(mBrowseButton, &QToolButton::clicked, this,
        &FileBrowserWindow::browseDirectory);
    connect(mShowInFileManagerButton, &QToolButton::clicked, this,
        &FileBrowserWindow::showInFileManager);
    connect(mFileSystemTree, &QTreeView::activated, this,
        &FileBrowserWindow::itemActivated);
    connect(&Singletons::fileDialog(), &FileDialog::directoryChanged, this,
        &FileBrowserWindow::currentDirectoryChanged);
    connect(mRootDirectory, &QComboBox::currentTextChanged, this,
        &FileBrowserWindow::setRootPath);
}

void FileBrowserWindow::setRootPath(const QString &path)
{
    const auto index = mModel->setRootPath(path);
    if (index != mFileSystemTree->rootIndex()) {
        mFileSystemTree->setRootIndex(index);
        mFileSystemTree->collapseAll();
        mFileSystemTree->clearSelection();

        updateRecentDirectories(path);
    }
}

bool FileBrowserWindow::revealDirectory(const QDir &dir)
{
    const auto native = toNativeCanonicalFilePath(dir.absolutePath());
    const auto index = mModel->index(native);
    mFileSystemTree->setExpanded(index, true);
    if (!mFileSystemTree->contentsRect().intersects(
            mFileSystemTree->visualRect(index))) {
        mFileSystemTree->scrollTo(index, QTreeView::PositionAtTop);
        if (mFileSystemTree->visualRect(index).isEmpty())
            return false;
    }
    return true;
}

void FileBrowserWindow::focusDirectory(const QDir &dir)
{
    const auto path = dir.absolutePath();
    mFileSystemTree->setCurrentIndex(mModel->index(path));
}

void FileBrowserWindow::currentDirectoryChanged(const QDir &dir)
{
    if (!mFileSystemTree->rootIndex().isValid())
        setRootPath(dir.absolutePath());

    if (!revealDirectory(dir)) {
        auto path = completeToRecentDirectory(dir.absolutePath());
        setRootPath(path);
        revealDirectory(dir);
        focusDirectory(dir);
    }
}

void FileBrowserWindow::itemActivated(const QModelIndex &index)
{
    Q_EMIT fileActivated(toNativeCanonicalFilePath(mModel->filePath(index)));
}

void FileBrowserWindow::browseDirectory()
{
    auto options = FileDialog::Options{};
    options.setFlag(FileDialog::Directory);
    if (Singletons::fileDialog().exec(options))
        setRootPath(Singletons::fileDialog().fileName());
}

void FileBrowserWindow::showInFileManager()
{
    auto path = mModel->rootPath();
    const auto &selectedIndexes =
        mFileSystemTree->selectionModel()->selectedIndexes();
    if (!selectedIndexes.isEmpty())
        path = mModel->filePath(selectedIndexes.first());
    ::showInFileManager(path);
}

void FileBrowserWindow::updateRecentDirectories(const QString &path)
{
    auto native = toNativeCanonicalFilePath(path);
    auto paths = mRecentDirectories->stringList();
    paths.removeAll(native);
    paths.insert(0, native);
    mRecentDirectories->setStringList(paths);
}

QString FileBrowserWindow::completeToRecentDirectory(const QString &path)
{
    auto native = toNativeCanonicalFilePath(path);
    const auto &recentPaths = mRecentDirectories->stringList();
    for (const auto &recent : recentPaths)
        if (native.startsWith(recent))
            return recent;
    return native;
}
