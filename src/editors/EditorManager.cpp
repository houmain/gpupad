#include "EditorManager.h"
#include "Singletons.h"
#include "FileDialog.h"
#include <functional>
#include <QDockWidget>
#include <QMessageBox>
#include <QAction>
#include <QApplication>
#include <QMimeData>

EditorManager::EditorManager(QWidget *parent)
    : DockWindow(parent)
    , mFindReplaceBar(new FindReplaceBar(this))
{
    setWindowFlags(Qt::Widget);
    setTabPosition(Qt::AllDockWidgetAreas, QTabWidget::North);
    setDockOptions(AnimatedDocks | AllowNestedDocks | AllowTabbedDocks);
    setDocumentMode(true);
    setContentsMargins(0, 1, 0, 0);
    setAcceptDrops(true);
}

EditorManager::~EditorManager() = default;

int EditorManager::openNotSavedDialog(const QString& fileName)
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

void EditorManager::dragEnterEvent(QDragEnterEvent *e)
{
    const auto urls = e->mimeData()->urls();
    if (!urls.isEmpty() && !urls.front().toLocalFile().isEmpty())
        e->accept();
}

void EditorManager::dropEvent(QDropEvent *e)
{
    const auto urls = e->mimeData()->urls();
    for (const QUrl &url : urls)
        openEditor(url.toLocalFile());
}

void EditorManager::updateCurrentEditor()
{
    auto previous = mCurrentDock;
    mCurrentDock = nullptr;
    auto focusWidget = qApp->focusWidget();
    foreach (QDockWidget* dock, mDocks.keys()) {
        if (dock->isAncestorOf(focusWidget)) {
            mCurrentDock = dock;
            updateDockCurrentProperty(dock, true);
            emit sourceTypeChanged(currentSourceType());
            break;
        }
    }
    if (previous != mCurrentDock)
        updateDockCurrentProperty(previous, false);
}

QString EditorManager::currentEditorFileName()
{
    if (auto editor = currentEditor())
        return editor->fileName();
    return "";
}

IEditor *EditorManager::currentEditor()
{
    if (mCurrentDock)
        return mDocks[mCurrentDock];
    return nullptr;
}

SourceType EditorManager::currentSourceType()
{
    if (auto editor = currentEditor())
        return editor->sourceType();
    return SourceType::None;
}

void EditorManager::setCurrentSourceType(SourceType sourceType)
{
    if (auto editor = currentEditor()) {
        editor->setSourceType(sourceType);
        emit sourceTypeChanged(sourceType);
    }
}

QList<QMetaObject::Connection> EditorManager::connectEditActions(
    const EditActions &actions)
{
    if (auto editor = currentEditor())
        return editor->connectEditActions(actions);
    return { };
}

QString EditorManager::openNewSourceEditor(const QString &baseName,
    SourceType sourceType)
{
    auto fileName = FileDialog::generateNextUntitledFileName(baseName);
    auto editor = new SourceEditor(fileName, mFindReplaceBar);
    editor->setSourceType(sourceType);
    addSourceEditor(editor);
    autoRaise(editor);
    return fileName;
}

QString EditorManager::openNewBinaryEditor(const QString &baseName)
{
    auto fileName = FileDialog::generateNextUntitledFileName(baseName);
    auto editor = new BinaryEditor(fileName);
    addBinaryEditor(editor);
    autoRaise(editor);
    return fileName;
}

QString EditorManager::openNewTextureEditor(const QString &baseName)
{
    auto fileName = FileDialog::generateNextUntitledFileName(baseName);
    auto editor = new TextureEditor(fileName);
    addTextureEditor(editor);
    autoRaise(editor);
    return fileName;
}

bool EditorManager::openEditor(const QString &fileName,
    bool asBinaryFile)
{
    if (!asBinaryFile && openTextureEditor(fileName))
        return true;
    if (!asBinaryFile && openSourceEditor(fileName))
        return true;
    if (openBinaryEditor(fileName))
        return true;

    return false;
}

SourceEditor *EditorManager::openSourceEditor(const QString &fileName, 
    int line, int column)
{
    auto editor = getSourceEditor(fileName);
    if (!editor) {
        editor = new SourceEditor(fileName, mFindReplaceBar);
        if (!editor->load()) {
            delete editor;
            return nullptr;
        }
        addSourceEditor(editor);
    }
    autoRaise(editor);
    if (editor)
        editor->setCursorPosition(line, column);
    return editor;
}

BinaryEditor *EditorManager::openBinaryEditor(const QString &fileName)
{
    auto editor = getBinaryEditor(fileName);
    if (!editor) {
        editor = new BinaryEditor(fileName);
        if (!editor->load()) {
            delete editor;
            return nullptr;
        }
        addBinaryEditor(editor);
    }
    autoRaise(editor);
    return editor;
}

TextureEditor *EditorManager::openTextureEditor(const QString &fileName)
{
    auto editor = getTextureEditor(fileName);
    if (!editor) {
        editor = new TextureEditor(fileName);
        if (!editor->load()) {
            delete editor;
            return nullptr;
        }
        addTextureEditor(editor);
    }
    autoRaise(editor);
    return editor;
}

SourceEditor* EditorManager::getSourceEditor(const QString &fileName)
{
    foreach (SourceEditor *editor, mSourceEditors)
        if (editor->fileName() == fileName)
            return editor;
    return nullptr;
}

BinaryEditor* EditorManager::getBinaryEditor(const QString &fileName)
{
    foreach (BinaryEditor *editor, mBinaryEditors)
        if (editor->fileName() == fileName)
            return editor;
    return nullptr;
}

TextureEditor* EditorManager::getTextureEditor(const QString &fileName)
{
    foreach (TextureEditor *editor, mTextureEditors)
        if (editor->fileName() == fileName)
            return editor;
    return nullptr;
}

QStringList EditorManager::getSourceFileNames() const
{
    auto result = QStringList();
    foreach (SourceEditor *editor, mSourceEditors)
        result.append(editor->fileName());
    return result;
}

QStringList EditorManager::getBinaryFileNames() const
{
    auto result = QStringList();
    foreach (BinaryEditor *editor, mBinaryEditors)
        result.append(editor->fileName());
    return result;
}

QStringList EditorManager::getImageFileNames() const
{
    auto result = QStringList();
    foreach (TextureEditor *editor, mTextureEditors)
        result.append(editor->fileName());
    return result;
}

bool EditorManager::saveEditor()
{
    if (auto editor = currentEditor()) {
        if (FileDialog::isUntitled(editor->fileName()) ||
            !editor->save())
            return saveEditorAs();
        return true;
    }
    return false;
}

bool EditorManager::saveEditorAs()
{
    if (!mCurrentDock)
        return false;

    if (auto editor = currentEditor()) {
        auto options = FileDialog::Options{ FileDialog::Saving };
        if (qobject_cast<SourceEditor*>(mCurrentDock->widget())) {
            options |= FileDialog::ShaderExtensions;
            options |= FileDialog::ScriptExtensions;
        }
        else if (qobject_cast<BinaryEditor*>(mCurrentDock->widget()))
            options |= FileDialog::BinaryExtensions;
        else if (qobject_cast<TextureEditor*>(mCurrentDock->widget()))
            options |= FileDialog::TextureExtensions;

        if (Singletons::fileDialog().exec(options, editor->fileName())) {
            auto prevFileName = editor->fileName();
            editor->setFileName(Singletons::fileDialog().fileName());
            emit editorRenamed(prevFileName, editor->fileName());
            return editor->save();
        }
    }
    return false;
}

bool EditorManager::saveAllEditors()
{
    foreach (QDockWidget *dock, mDocks.keys())
        if (dock->isWindowModified())
            if (!saveDock(dock))
                return false;
    return true;
}

bool EditorManager::reloadEditor()
{
    if (auto editor = currentEditor()) {
        if (FileDialog::isUntitled(editor->fileName()))
            return false;

        return editor->load();
    }
    return false;
}

bool EditorManager::closeEditor()
{
    if (mCurrentDock)
        return closeDock(mCurrentDock);
    return false;
}

bool EditorManager::closeAllEditors()
{
    foreach (QDockWidget* dock, mDocks.keys())
        if (!closeDock(dock))
            return false;
    return true;
}

bool EditorManager::closeAllTextureEditors()
{
    foreach (QDockWidget* dock, mDocks.keys())
        if (qobject_cast<TextureEditor*>(dock->widget()))
            if (!closeDock(dock))
                return false;
    return true;
}

void EditorManager::addSourceEditor(SourceEditor *editor)
{
    mSourceEditors.append(editor);

    auto dock = createDock(editor, editor);
    connect(editor, &SourceEditor::modificationChanged,
        dock, &QDockWidget::setWindowModified);
    connect(editor, &SourceEditor::fileNameChanged,
        [dock](const QString &fileName) {
            dock->setWindowTitle(FileDialog::getWindowTitle(fileName));
        });
    connect(editor, &SourceEditor::textChanged,
        [this, editor]() {
            emit editorChanged(editor->fileName());
        });
}

void EditorManager::addTextureEditor(TextureEditor *editor)
{
    mTextureEditors.append(editor);

    auto dock = createDock(editor, editor);
    connect(editor, &TextureEditor::modificationChanged,
        dock, &QDockWidget::setWindowModified);
    connect(editor, &TextureEditor::fileNameChanged,
        [dock](const QString &fileName) {
            dock->setWindowTitle(FileDialog::getWindowTitle(fileName));
        });
    connect(editor, &TextureEditor::dataChanged,
        [this, editor]() {
            emit editorChanged(editor->fileName());
        });
}

void EditorManager::addBinaryEditor(BinaryEditor *editor)
{
    mBinaryEditors.append(editor);

    auto dock = createDock(editor, editor);
    connect(editor, &BinaryEditor::modificationChanged,
        dock, &QDockWidget::setWindowModified);
    connect(editor, &BinaryEditor::fileNameChanged,
        [dock](const QString &fileName) {
            dock->setWindowTitle(FileDialog::getWindowTitle(fileName));
        });
    connect(editor, qOverload<>(&BinaryEditor::dataChanged),
        [this, editor]() { emit editorChanged(editor->fileName()); });
}

QDockWidget *EditorManager::createDock(QWidget *widget, IEditor *editor)
{
    auto fileName = editor->fileName();
    auto dock = new QDockWidget(FileDialog::getWindowTitle(fileName), this);
    dock->setWidget(widget);

    dock->setFeatures(QDockWidget::DockWidgetMovable |
        QDockWidget::DockWidgetClosable | 
        QDockWidget::DockWidgetFloatable);
    dock->toggleViewAction()->setVisible(false);
    dock->installEventFilter(this);

    auto tabified = false;
    foreach (QDockWidget* d, mDocks.keys()) {
        if (!d->isFloating() &&
              mDocks[d]->tabifyGroup() == editor->tabifyGroup()) {
            tabifyDockWidget(d, dock);
            tabified = true;
            break;
        }
    }
    if (!tabified) {
        addDockWidget(Qt::TopDockWidgetArea, dock);
        resizeDocks({ dock }, { width() }, Qt::Horizontal);
    }

    mDocks.insert(dock, editor);
    return dock;
}

bool EditorManager::saveDock(QDockWidget *dock)
{
    auto currentDock = mCurrentDock;
    mCurrentDock = dock;
    auto result = saveEditor();
    mCurrentDock = currentDock;
    return result;
}

bool EditorManager::closeDock(QDockWidget *dock)
{
    auto editor = mDocks[dock];

    if (dock->isWindowModified()) {
        auto ret = openNotSavedDialog(editor->fileName());
        if (ret == QMessageBox::Cancel)
            return false;

        if (ret == QMessageBox::Save &&
            !saveDock(dock))
            return false;
    }

    emit editorRenamed(editor->fileName(), "");

    mSourceEditors.removeAll(static_cast<SourceEditor*>(editor));
    mBinaryEditors.removeAll(static_cast<BinaryEditor*>(editor));
    mTextureEditors.removeAll(static_cast<TextureEditor*>(editor));

    mDocks.remove(dock);

    if (mCurrentDock == dock) {
        updateDockCurrentProperty(dock, false);
        mCurrentDock = nullptr;
        if (!mDocks.isEmpty())
            autoRaise(mDocks.lastKey()->widget());
    }

    DockWindow::closeDock(dock);

    if (mDocks.isEmpty())
        Singletons::fileDialog().resetNextUntitledFileIndex();

    return true;
}

void EditorManager::autoRaise(QWidget *editor)
{
    if (mAutoRaise && editor)
        raiseDock(qobject_cast<QDockWidget*>(editor->parentWidget()));
}

void updateDockCurrentProperty(QDockWidget *dock, bool current)
{
    if (dock && dock->property("current") != current) {
        dock->setProperty("current", current);
        if (auto frame = qobject_cast<QFrame*>(dock->widget())) {
            frame->style()->unpolish(frame);
            frame->style()->polish(frame);
            frame->update();
        }
    }
}
