#include "FileManager.h"
#include "SourceEditor.h"
#include "GlslHighlighter.h"
#include "BinaryEditor.h"
#include "ImageEditor.h"
#include "SourceFile.h"
#include "BinaryFile.h"
#include "ImageFile.h"
#include "Singletons.h"
#include "FileDialog.h"
#include <functional>
#include <QDockWidget>
#include <QTimer>
#include <QMessageBox>
#include <QAction>
#include <QApplication>

namespace {
    void raiseEditor(QWidget *editor)
    {
        if (!editor)
            return;
        QTimer::singleShot(0,
            [editor]() {
                auto dock = qobject_cast<QDockWidget*>(editor->parentWidget());
                if (dock)
                    dock->raise();
                editor->setFocus();
            });
    }
} // namespace

struct FileManager::EditorFunctions
{
    std::function<void()> remove;
    std::function<QList<QMetaObject::Connection>(const EditActions&)>
        connectEditActions;
    std::function<QString()> fileName;
    std::function<void(const QString&)> setFileName;
    std::function<bool()> load;
    std::function<bool()> save;
};

SourceFilePtr FileManager::openSourceFile(const QString &fileName)
{
    if (fileName.isEmpty())
        return nullptr;

    auto file = SourceFilePtr(new SourceFile());
    file->setFileName(fileName);
    return (file->load() ? file : SourceFilePtr());
}

BinaryFilePtr FileManager::openBinaryFile(const QString &fileName)
{
    if (fileName.isEmpty())
        return nullptr;

    auto file = BinaryFilePtr(new BinaryFile());
    file->setFileName(fileName);
    return (file->load() ? file : BinaryFilePtr());
}

ImageFilePtr FileManager::openImageFile(const QString &fileName, int index)
{
    Q_UNUSED(index);
    if (fileName.isEmpty())
        return nullptr;

    auto file = ImageFilePtr(new ImageFile());
    file->setFileName(fileName);
    return (file->load() ? file : ImageFilePtr());
}

FileManager::FileManager(QWidget *parent)
    : DockWindow(parent)
{
    setWindowFlags(Qt::Widget);
    setTabPosition(Qt::AllDockWidgetAreas, QTabWidget::North);
    setDockOptions(AnimatedDocks | AllowNestedDocks |
        AllowTabbedDocks | GroupedDragging);
    setDocumentMode(true);
    setContentsMargins(0, 1, 0, 0);
}

FileManager::~FileManager() = default;

int FileManager::openNotSavedDialog(const QString& fileName)
{
    QMessageBox dialog(this);
    dialog.setIcon(QMessageBox::Question);
    dialog.setWindowTitle(tr("Question"));
    dialog.setText(tr("<h3>The file '%1' is not saved.</h3>"
           "Do you want to save it before closing?<br>")
           .arg(FileDialog::getFileTitle(fileName)));
    dialog.addButton(QMessageBox::Save);
    dialog.addButton(tr("&Don't Save"), QMessageBox::RejectRole);
    dialog.addButton(QMessageBox::Cancel);
    dialog.setDefaultButton(QMessageBox::Save);
    return dialog.exec();
}

bool FileManager::updateCurrentEditor()
{
    mCurrentDock = nullptr;

    auto focus = qApp->focusWidget();
    foreach (QDockWidget* dock, mDocks.keys()) {
        if (dock->isAncestorOf(focus)) {
            mCurrentDock = dock;
            break;
        }
    }

    if (mCurrentDock != mPrevCurrentDock) {
        mPrevCurrentDock = mCurrentDock;
        return true;
    }
    return false;
}

QString FileManager::currentEditorFileName()
{
    if (auto editor = currentEditorFunctions())
        return editor->fileName();
    return "";
}

auto FileManager::currentEditorFunctions() -> EditorFunctions*
{
    if (mCurrentDock)
        return &mDocks[mCurrentDock];
    return nullptr;
}

QList<QMetaObject::Connection> FileManager::connectEditActions(
    const EditActions &actions)
{
    if (auto editor = currentEditorFunctions())
        return editor->connectEditActions(actions);
    return { };
}

QString FileManager::openNewSourceEditor()
{
    auto file = SourceFilePtr(new SourceFile());
    auto fileName = FileDialog::generateNextUntitledFileName();
    file->setFileName(fileName);
    raiseEditor(createSourceEditor(file));
    return fileName;
}

QString FileManager::openNewBinaryEditor()
{
    auto file = BinaryFilePtr(new BinaryFile());
    auto fileName = FileDialog::generateNextUntitledFileName();
    file->setFileName(fileName);
    raiseEditor(createBinaryEditor(file));
    return fileName;
}

QString FileManager::openNewImageEditor()
{
    auto file = ImageFilePtr(new ImageFile());
    auto fileName = FileDialog::generateNextUntitledFileName();
    file->setFileName(fileName);
    raiseEditor(createImageEditor(file));
    return fileName;
}

bool FileManager::openEditor(const QString &fileName, bool raise)
{
    if (FileDialog::isBinaryFileName(fileName))
        if (openBinaryEditor(fileName, raise))
            return true;

    if (openSourceEditor(fileName, raise))
        return true;
    if (openImageEditor(fileName, raise))
        return true;
    if (openBinaryEditor(fileName, raise))
        return true;

    return false;
}

SourceEditor *FileManager::openSourceEditor(const QString &fileName, bool raise,
    int line, int column)
{
    auto editor = findSourceEditor(fileName);
    if (!editor)
        if (auto sourceFile = openSourceFile(fileName))
            editor = createSourceEditor(sourceFile);
    if (raise)
        raiseEditor(editor);
    if (editor)
        editor->setCursorPosition(line, column);
    return editor;
}

BinaryEditor *FileManager::openBinaryEditor(const QString &fileName, bool raise)
{
    auto editor = findBinaryEditor(fileName);
    if (!editor)
        if (auto file = openBinaryFile(fileName))
            editor = createBinaryEditor(file);
    if (raise)
        raiseEditor(editor);
    return editor;
}

ImageEditor *FileManager::openImageEditor(const QString &fileName, bool raise)
{
    auto editor = findImageEditor(fileName);
    if (!editor)
        if (auto file = openImageFile(fileName, 0))
            editor = createImageEditor({ file });
    if (raise)
        raiseEditor(editor);
    return editor;
}

SourceEditor* FileManager::findSourceEditor(const QString &fileName)
{
    foreach (SourceEditor *editor, mSourceEditors)
        if (editor->file()->fileName() == fileName)
            return editor;
    return nullptr;
}

BinaryEditor* FileManager::findBinaryEditor(const QString &fileName)
{
    foreach (BinaryEditor *editor, mBinaryEditors)
        if (editor->file()->fileName() == fileName)
            return editor;
    return nullptr;
}

ImageEditor* FileManager::findImageEditor(const QString &fileName)
{
    foreach (ImageEditor *editor, mImageEditors)
        if (editor->file()->fileName() == fileName)
            return editor;
    return nullptr;
}

SourceFilePtr FileManager::findSourceFile(const QString &fileName)
{
    if (auto editor = findSourceEditor(fileName))
        return editor->file();
    return nullptr;
}

BinaryFilePtr FileManager::findBinaryFile(const QString &fileName)
{
    if (auto editor = findBinaryEditor(fileName))
        return editor->file();
    return nullptr;
}

ImageFilePtr FileManager::findImageFile(const QString &fileName, int index)
{
    Q_UNUSED(index);
    if (auto editor = findImageEditor(fileName))
        return editor->file();
    return nullptr;
}

QStringList FileManager::getSourceFileNames() const
{
    auto result = QStringList();
    for (auto editor : mSourceEditors)
        result.append(editor->file()->fileName());
    return result;
}

QStringList FileManager::getBinaryFileNames() const
{
    auto result = QStringList();
    for (auto editor : mBinaryEditors)
        result.append(editor->file()->fileName());
    return result;
}

QStringList FileManager::getImageFileNames() const
{
    auto result = QStringList();
    for (auto editor : mImageEditors)
        result.append(editor->file()->fileName());
    return result;
}

bool FileManager::saveEditor()
{
    if (auto editor = currentEditorFunctions()) {
        if (FileDialog::isUntitled(editor->fileName()))
            return saveEditorAs();

        return editor->save();
    }
    return false;
}

bool FileManager::saveEditorAs()
{
    if (!mCurrentDock)
        return false;

    if (auto editor = currentEditorFunctions()) {
        auto options = FileDialog::Options{ FileDialog::Saving };
        if (qobject_cast<SourceEditor*>(mCurrentDock->widget()))
            options |= FileDialog::ShaderExtensions;
        else if (qobject_cast<BinaryEditor*>(mCurrentDock->widget()))
            options |= FileDialog::BinaryExtensions;
        else if (qobject_cast<ImageEditor*>(mCurrentDock->widget()))
            options |= FileDialog::ImageExtensions;

        if (Singletons::fileDialog().exec(options, editor->fileName())) {
            auto prevFileName = editor->fileName();
            editor->setFileName(Singletons::fileDialog().fileName());
            emit editorRenamed(prevFileName, editor->fileName());
            return saveEditor();
        }
    }
    return false;
}

bool FileManager::saveAllEditors()
{
    foreach (QDockWidget *dock, mDocks.keys())
        if (dock->isWindowModified())
            if (!saveDock(dock))
                return false;
    return true;
}

bool FileManager::reloadEditor()
{
    if (auto editor = currentEditorFunctions()) {
        if (FileDialog::isUntitled(editor->fileName()))
            return false;

        return editor->load();
    }
    return false;
}

bool FileManager::closeEditor()
{
    if (mCurrentDock)
        return closeDock(mCurrentDock);
    return false;
}

bool FileManager::closeAllEditors()
{
    foreach (QDockWidget* dock, mDocks.keys())
        if (!closeDock(dock))
            return false;
    return true;
}

SourceEditor *FileManager::createSourceEditor(QSharedPointer<SourceFile> file)
{
    auto highlighter = new GlslHighlighter(&file->document());
    auto completer = highlighter->completer();
    auto editor = new SourceEditor(file, highlighter, completer, this);
    auto dock = new QDockWidget(
        FileDialog::getWindowTitle(file->fileName()), this);
    connect(editor, &SourceEditor::modificationChanged,
        dock, &QDockWidget::setWindowModified);
    connect(file.data(), &SourceFile::fileNameChanged,
        [dock](const QString &fileName) {
            dock->setWindowTitle(FileDialog::getWindowTitle(fileName));
        });
    connect(editor, &SourceEditor::textChanged,
        [this, editor]() {
            emit sourceEditorChanged(editor->file()->fileName());
        });

    mSourceEditors.append(editor);
    addDock(dock, editor, {
        [this, editor]() { mSourceEditors.removeAll(editor); },
        [editor](const EditActions& actions)
            { return editor->connectEditActions(actions); },
        [editor]() { return editor->file()->fileName(); },
        [editor](const QString& fileName)
            { return editor->file()->setFileName(fileName); },
        [editor]() { return editor->file()->load(); },
        [editor]() { return editor->file()->save(); }
    });
    return editor;
}

ImageEditor *FileManager::createImageEditor(QSharedPointer<ImageFile> file)
{
    auto editor = new ImageEditor(file, this);
    auto dock = new QDockWidget(
        FileDialog::getWindowTitle(file->fileName()), this);
    connect(file.data(), &ImageFile::modificationChanged,
        dock, &QDockWidget::setWindowModified);
    connect(file.data(), &ImageFile::fileNameChanged,
        [dock](const QString &fileName) {
            dock->setWindowTitle(FileDialog::getWindowTitle(fileName));
        });
    connect(file.data(), &ImageFile::dataChanged,
        [this, editor]() {
            emit imageEditorChanged(editor->file()->fileName());
        });

    mImageEditors.append(editor);
    addDock(dock, editor, {
        [this, editor]() { mImageEditors.removeAll(editor); },
        [editor](const EditActions& actions)
            { return editor->connectEditActions(actions); },
        [editor]() { return editor->file()->fileName(); },
        [editor](const QString& fileName)
            { return editor->file()->setFileName(fileName); },
        [editor]() { return editor->file()->load(); },
        [editor]() { return editor->file()->save(); }
    });
    return editor;
}

BinaryEditor *FileManager::createBinaryEditor(QSharedPointer<BinaryFile> file)
{
    auto editor = new BinaryEditor(file, this);
    auto dock = new QDockWidget(
        FileDialog::getWindowTitle(file->fileName()), this);
    connect(file.data(), &BinaryFile::modificationChanged,
        dock, &QDockWidget::setWindowModified);
    connect(file.data(), &BinaryFile::fileNameChanged,
        [dock](const QString &fileName) {
            dock->setWindowTitle(FileDialog::getWindowTitle(fileName));
        });
    connect(file.data(), &BinaryFile::dataChanged,
        [this, editor]() {
            emit binaryEditorChanged(editor->file()->fileName());
        });

    mBinaryEditors.append(editor);
    addDock(dock, editor, {
        [this, editor]() { mBinaryEditors.removeAll(editor); },
        [editor](const EditActions& actions)
            { return editor->connectEditActions(actions); },
        [editor]() { return editor->file()->fileName(); },
        [editor](const QString& fileName)
            { return editor->file()->setFileName(fileName); },
        [editor]() { return editor->file()->load(); },
        [editor]() { return editor->file()->save(); }
    });
    return editor;
}

void FileManager::addDock(QDockWidget *dock, QWidget *widget,
    EditorFunctions functions)
{
    dock->setWidget(widget);

    dock->setFeatures(QDockWidget::DockWidgetMovable |
        QDockWidget::DockWidgetClosable | 
        QDockWidget::DockWidgetFloatable);

    dock->toggleViewAction()->setVisible(false);
    dock->installEventFilter(this);

    if (!mDocks.isEmpty())
        tabifyDockWidget(mDocks.firstKey(), dock);
    else
        addDockWidget(Qt::LeftDockWidgetArea, dock);

    mDocks.insert(dock, std::move(functions));
}

bool FileManager::saveDock(QDockWidget *dock)
{
    auto currentDock = mCurrentDock;
    mCurrentDock = dock;
    auto result = saveEditor();
    mCurrentDock = currentDock;
    return result;
}

bool FileManager::closeDock(QDockWidget *dock)
{
    auto& editor = mDocks[dock];

    if (dock->isWindowModified()) {
        auto ret = openNotSavedDialog(editor.fileName());
        if (ret == QMessageBox::Cancel)
            return false;

        if (ret == QMessageBox::Save &&
            !saveDock(dock))
            return false;
    }

    emit editorRenamed(editor.fileName(), "");

    editor.remove();
    mDocks.remove(dock);

    if (mCurrentDock == dock) {
        mCurrentDock = nullptr;
        if (!mDocks.isEmpty())
            raiseEditor(mDocks.lastKey()->widget());
    }

    DockWindow::closeDock(dock);

    if (mDocks.isEmpty()) {
        Singletons::fileDialog().resetNextUntitledFileIndex();
        openNewSourceEditor();
    }
    return true;
}
