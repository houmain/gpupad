#include "EditorManager.h"
#include "Singletons.h"
#include "FileCache.h"
#include "FileDialog.h"
#include "SynchronizeLogic.h"
#include "binary/BinaryEditor.h"
#include "binary/BinaryEditorToolBar.h"
#include "source/SourceEditor.h"
#include "source/SourceEditorToolBar.h"
#include "texture/TextureEditor.h"
#include "texture/TextureEditorToolBar.h"
#include "texture/TextureInfoBar.h"
#include "qml/QmlView.h"
#include <functional>
#include <QDockWidget>
#include <QAction>
#include <QApplication>
#include <QToolBar>
#include <QToolButton>
#include <QBoxLayout>
#include <QRandomGenerator>
#include <QClipboard>
#include <QMimeData>

EditorManager::EditorManager(QWidget *parent)
    : DockWindow(parent)
    , mFindReplaceBar(new FindReplaceBar(this))
    , mTextureInfoBar(new TextureInfoBar(this))
{
    setWindowFlags(Qt::Widget);
    setTabPosition(Qt::AllDockWidgetAreas, QTabWidget::North);
    setDockOptions(AnimatedDocks | AllowNestedDocks | AllowTabbedDocks);
    setDocumentMode(true);
    setContentsMargins(0, 1, 0, 0);

    connect(this, &DockWindow::dockCloseRequested, this, 
        [this](QDockWidget *dock) { 
            if (promptSaveDock(dock)) 
                closeDock(dock); 
        });
    connect(QApplication::clipboard(), &QClipboard::changed,
        [this]() { Q_EMIT canPasteInNewEditorChanged(canPasteInNewEditor()); });
}

EditorManager::~EditorManager() = default;

QWidget *EditorManager::createEditorPropertiesPanel(
    QAction *showAction)
{
    auto propertiesPanel = new QWidget(this);
    propertiesPanel->setAutoFillBackground(true);
    propertiesPanel->setBackgroundRole(QPalette::ToolTipBase);
    propertiesPanel->hide();

    auto layout = new QHBoxLayout(propertiesPanel);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);
    layout->addWidget(mFindReplaceBar);
    layout->addWidget(mTextureInfoBar);
    layout->setAlignment(mFindReplaceBar, Qt::AlignTop);
    layout->setAlignment(mTextureInfoBar, Qt::AlignTop);

    const auto showPropertiesPanel = [this, showAction, propertiesPanel]() {
        propertiesPanel->show();
        updateEditorPropertiesVisibility();
        showAction->setChecked(true);
    };
    const auto hidePropertiesPanel = [this, showAction, propertiesPanel]() {
        propertiesPanel->hide();
        updateEditorPropertiesVisibility();
        showAction->setChecked(false);
    };
    connect(showAction, &QAction::toggled, [=](bool show) { 
        // make keyboard shortcut show instead of toggle
        if (qApp->keyboardModifiers()) {
            showAction->setChecked(true);
            show = true;
        }
        (show ? showPropertiesPanel() : hidePropertiesPanel()); 
    });
    connect(mFindReplaceBar, &FindReplaceBar::cancelled, hidePropertiesPanel);
    connect(mTextureInfoBar, &TextureInfoBar::cancelled, hidePropertiesPanel);

    return propertiesPanel;
}

void EditorManager::createEditorToolBars(QToolBar *mainToolBar) 
{
    mTextureEditorToolBar = new TextureEditorToolBar(this);
    mainToolBar->addWidget(mTextureEditorToolBar);

    mBinaryEditorToolBar = new BinaryEditorToolBar(this);
    mainToolBar->addWidget(mBinaryEditorToolBar);

    mSourceEditorToolBar = new SourceEditorToolBar(this);
    mainToolBar->addWidget(mSourceEditorToolBar);

    connect(mSourceEditorToolBar, &SourceEditorToolBar::validateSourceChanged,
        &Singletons::synchronizeLogic(), &SynchronizeLogic::setValidateSource);
    connect(mSourceEditorToolBar, &SourceEditorToolBar::sourceTypeChanged,
        &Singletons::synchronizeLogic(), &SynchronizeLogic::setCurrentEditorSourceType);
    connect(this, &EditorManager::currentEditorChanged,
        &Singletons::synchronizeLogic(), &SynchronizeLogic::setCurrentEditorFileName);

    updateEditorToolBarVisibility();
    updateEditorPropertiesVisibility();
}

void EditorManager::setEditorToolBarPalette(const QPalette &palette)
{
    mTextureEditorToolBar->setPalette(palette);
    mBinaryEditorToolBar->setPalette(palette);
    mSourceEditorToolBar->setPalette(palette);
}

void EditorManager::updateEditorToolBarVisibility()
{
    // setting maximumWidth since simply setting visibility did not work
    const auto setVisible = [](QWidget* widget, bool visible) {
        widget->setMaximumWidth(visible ? 65536 : 0);
    };
    setVisible(mTextureEditorToolBar, 
        mCurrentDock && qobject_cast<TextureEditor*>(mCurrentDock->widget()));
    setVisible(mBinaryEditorToolBar,
        mCurrentDock && qobject_cast<BinaryEditor*>(mCurrentDock->widget()));
    setVisible(mSourceEditorToolBar,
        mCurrentDock && qobject_cast<SourceEditor*>(mCurrentDock->widget()));
}

void EditorManager::updateEditorPropertiesVisibility()
{
    // do not hide when non-editor dock is selected
    mTextureInfoBar->setEnabled(hasCurrentEditor());
    if (hasEditor() && !hasCurrentEditor())
        return;

    const auto widget = (mCurrentDock ? mCurrentDock->widget() : nullptr);
    mFindReplaceBar->setVisible(qobject_cast<SourceEditor*>(widget));
    mTextureInfoBar->setVisible(qobject_cast<TextureEditor*>(widget));
    mTextureInfoBar->setPickerEnabled(
        mTextureInfoBar->parentWidget()->isVisible());
}

int EditorManager::getFocusedEditorIndex() const
{
    auto index = 0;
    for (auto [dock, editor] : mDocks) {
        if (dock == mCurrentDock)
            return index;
        ++index;
    }
    return -1;
}

bool EditorManager::focusEditorByIndex(int index, bool wrap)
{
    const auto count = static_cast<int>(mDocks.size());
    if (wrap)
        index = (index + count) % count;
    if (index < 0 || index >= count)
        return false;
    raiseDock(std::next(mDocks.begin(), index)->first);
    return true;
}

bool EditorManager::focusNextEditor(bool wrap)
{
    return focusEditorByIndex(getFocusedEditorIndex() + 1, wrap);
}

bool EditorManager::focusPreviousEditor(bool wrap)
{
    const auto current = getFocusedEditorIndex();
    return focusEditorByIndex((current == -1 ? 
        static_cast<int>(mDocks.size()) : current) - 1, wrap);
}

void EditorManager::updateCurrentEditor()
{
    auto previous = mCurrentDock;
    mCurrentDock = nullptr;
    auto focusWidget = qApp->focusWidget();
    for (const auto [dock, editor] : mDocks) {
        if (dock->isAncestorOf(focusWidget)) {
            mCurrentDock = dock;
            mLastFocusedTabifyGroupDock[editor->tabifyGroup()] = dock;
            updateDockCurrentProperty(dock, true);
            Q_EMIT currentEditorChanged(editor->fileName());
            break;
        }
    }
    if (previous != mCurrentDock)
        updateDockCurrentProperty(previous, false);

    updateEditorToolBarVisibility();
    updateEditorPropertiesVisibility();
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

QDockWidget *EditorManager::findEditorDock(const IEditor *editor) const
{
    for (auto [dock, dockEditor] : mDocks)
          if (editor == dockEditor)
              return dock;
    return nullptr;
}

QList<QMetaObject::Connection> EditorManager::connectEditActions(
    const EditActions &actions)
{
    if (auto editor = currentEditor())
        return editor->connectEditActions(actions);
    return { };
}

SourceEditor *EditorManager::openNewSourceEditor(const QString &fileName,
    SourceType sourceType)
{
    auto editor = new SourceEditor(fileName, mSourceEditorToolBar, mFindReplaceBar);
    editor->setSourceType(sourceType);
    addSourceEditor(editor);
    autoRaise(editor);
    editor->setCursorPosition(0, 0);
    return editor;
}

BinaryEditor *EditorManager::openNewBinaryEditor(const QString &fileName)
{
    auto editor = new BinaryEditor(fileName, mBinaryEditorToolBar);
    addBinaryEditor(editor);
    autoRaise(editor);
    return editor;
}

TextureEditor *EditorManager::openNewTextureEditor(const QString &fileName)
{
    auto editor = new TextureEditor(fileName, mTextureEditorToolBar, mTextureInfoBar);
    addTextureEditor(editor);
    autoRaise(editor);
    return editor;
}

void EditorManager::closeUntitledUntouchedSourceEditor()
{
    if (mCurrentDock && !mCurrentDock->isWindowModified()) {
        const auto fileName = mDocks[mCurrentDock]->fileName();
        if (FileDialog::isUntitled(fileName) && getSourceEditor(fileName))
            closeEditor();
    }
}

IEditor* EditorManager::openEditor(const QString &fileName,
    bool asBinaryFile)
{
    if (!asBinaryFile) {
        if (fileName.endsWith(".qml", Qt::CaseInsensitive)) {
            const auto modifiers = QApplication::queryKeyboardModifiers();
            if (!(modifiers & Qt::ControlModifier))
                if (auto editor = openQmlView(fileName))
                    return editor;
        }

        if (auto editor = openTextureEditor(fileName))
            return editor;
        if (auto editor = openSourceEditor(fileName))
            return editor;
    }
    return openBinaryEditor(fileName);
}

SourceEditor *EditorManager::openSourceEditor(const QString &fileName, 
    int line, int column)
{
    auto editor = getSourceEditor(fileName);
    if (!editor) {
        editor = new SourceEditor(fileName,
            mSourceEditorToolBar,  mFindReplaceBar);
        if (!editor->load()) {
            delete editor;
            return nullptr;
        }
        closeUntitledUntouchedSourceEditor();
        addSourceEditor(editor);
    }
    autoRaise(editor);
    if (line >= 0)
        editor->setCursorPosition(line, column);
    return editor;
}

BinaryEditor *EditorManager::openBinaryEditor(const QString &fileName)
{
    auto editor = getBinaryEditor(fileName);
    if (!editor) {
        editor = new BinaryEditor(fileName, mBinaryEditorToolBar);
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
        editor = new TextureEditor(fileName, mTextureEditorToolBar, mTextureInfoBar);
        if (!editor->load()) {
            delete editor;
            return nullptr;
        }
        addTextureEditor(editor);
    }
    autoRaise(editor);
    return editor;
}

QmlView *EditorManager::openQmlView(const QString &fileName)
{
    auto editor = getQmlView(fileName);
    if (!editor) {
        editor = new QmlView(fileName);
        if (!editor->load()) {
            delete editor;
            return nullptr;
        }
        mQmlViews.append(editor);
        createDock(editor, editor);
    }
    autoRaise(editor);
    return editor;
}

IEditor *EditorManager::getEditor(const QString &fileName)
{
    if (auto editor = getSourceEditor(fileName))
        return editor;
    if (auto editor = getBinaryEditor(fileName))
        return editor;
    return getTextureEditor(fileName);
}

SourceEditor* EditorManager::getSourceEditor(const QString &fileName)
{
    for (SourceEditor *editor : qAsConst(mSourceEditors))
        if (editor->fileName() == fileName)
            return editor;
    return nullptr;
}

BinaryEditor* EditorManager::getBinaryEditor(const QString &fileName)
{
    for (BinaryEditor *editor : qAsConst(mBinaryEditors))
        if (editor->fileName() == fileName)
            return editor;
    return nullptr;
}

TextureEditor* EditorManager::getTextureEditor(const QString &fileName)
{
    for (TextureEditor *editor : qAsConst(mTextureEditors))
        if (editor->fileName() == fileName)
            return editor;
    return nullptr;
}

QmlView* EditorManager::getQmlView(const QString &fileName)
{
    for (QmlView *editor : qAsConst(mQmlViews))
        if (editor->fileName() == fileName)
            return editor;
    return nullptr;
}

QStringList EditorManager::getSourceFileNames() const
{
    auto result = QStringList();
    for (SourceEditor *editor : mSourceEditors)
        result.append(editor->fileName());
    return result;
}

QStringList EditorManager::getBinaryFileNames() const
{
    auto result = QStringList();
    for (BinaryEditor *editor : mBinaryEditors)
        result.append(editor->fileName());
    return result;
}

QStringList EditorManager::getImageFileNames() const
{
    auto result = QStringList();
    for (TextureEditor *editor : mTextureEditors)
        result.append(editor->fileName());
    return result;
}

void EditorManager::renameEditors(const QString &prevFileName, const QString &fileName)
{
    if (prevFileName.isEmpty() ||
        fileName.isEmpty() ||
        prevFileName == fileName)
        return;

    for (auto [dock, editor] : mDocks)
        if (editor->fileName() == prevFileName)
            editor->setFileName(fileName);
}

void EditorManager::resetQmlViewsDependingOn(const QString &fileName)
{
    for (auto qmlView : qAsConst(mQmlViews))
        if (qmlView->dependsOn(fileName))
            qmlView->resetOnFocus();
}

bool EditorManager::saveEditor()
{
    if (auto editor = currentEditor()) {
        if (FileDialog::isUntitled(editor->fileName()))
            return saveEditorAs();

        Singletons::fileCache().handleEditorSave(editor->fileName());
        if (!editor->save()) {
            if (!showSavingFailedMessage(this, editor->fileName()))
                return false;
            return saveEditorAs();
        }
        return true;
    }
    return false;
}

bool EditorManager::saveEditorAs()
{
    if (auto editor = currentEditor()) {
        auto options = FileDialog::Options{ FileDialog::Saving };
        auto sourceType = SourceType{ };
        if (const auto editor = qobject_cast<SourceEditor*>(mCurrentDock->widget())) {
            options |= FileDialog::ShaderExtensions;
            options |= FileDialog::ScriptExtensions;
            sourceType = editor->sourceType();
        }
        else if (qobject_cast<BinaryEditor*>(mCurrentDock->widget())) {
            options |= FileDialog::BinaryExtensions;
        }
        else if (auto textureEditor = qobject_cast<TextureEditor*>(mCurrentDock->widget())) {
            options |= FileDialog::TextureExtensions;
            if (!textureEditor->texture().isConvertibleToImage())
                options |= FileDialog::SavingNon2DTexture;
        }

        const auto prevFileName = editor->fileName();
        while (Singletons::fileDialog().exec(options, editor->fileName(), sourceType)) {
            editor->setFileName(Singletons::fileDialog().fileName());
            if (!editor->save()) {
                if (!showSavingFailedMessage(this, editor->fileName()))
                    break;
                continue;
            }
            Q_EMIT editorRenamed(prevFileName, editor->fileName());
            return true;
        }
        editor->setFileName(prevFileName);
    }
    return false;
}

bool EditorManager::saveAllEditors()
{
    for (auto [dock, editor] : mDocks)
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

        // do not purge when reloading qml view
        if (mQmlViews.indexOf(static_cast<QmlView*>(editor)) < 0)
            Singletons::fileCache().invalidateFile(editor->fileName());

        return editor->load();
    }
    return false;
}

bool EditorManager::closeEditor()
{
    if (!mCurrentDock || !promptSaveDock(mCurrentDock))
        return false;
    closeDock(mCurrentDock);
    return true;
}

bool EditorManager::promptSaveAllEditors()
{
    for (auto [dock, editor] : mDocks)
        if (!promptSaveDock(dock))
            return false;
    return true;
}

bool EditorManager::closeAllEditors(bool promptSave)
{
    if (promptSave && !promptSaveAllEditors())
        return false;

    while (!mDocks.empty())
        closeDock(mDocks.begin()->first);

    return true;
}

bool EditorManager::closeAllTextureEditors()
{
    for (auto [dock, editor] : mDocks)
        if (qobject_cast<TextureEditor*>(dock->widget())) {
            if (!promptSaveDock(dock))
                return false;
            closeDock(dock);
        }
    return true;
}

QString EditorManager::getEditorObjectName(IEditor *editor) const
{
    if (auto dock = findEditorDock(editor))
        return dock->objectName();
    return "";
}

void EditorManager::setEditorObjectName(IEditor *editor, const QString &name)
{
    if (auto dock = findEditorDock(editor))
        dock->setObjectName(name);
}

bool EditorManager::canPasteInNewEditor() const 
{
    return (QApplication::clipboard()->mimeData() != nullptr);
}

void EditorManager::pasteInNewEditor()
{
    auto mimeData = QApplication::clipboard()->mimeData();
    if (!mimeData)
        return;

    const auto fileName = FileDialog::generateNextUntitledFileName(tr("Untitled"));
    closeUntitledUntouchedSourceEditor();

    if (mimeData->hasImage())
        if (auto editor = openNewTextureEditor(fileName)) {
            auto texture = TextureData();
            if (texture.loadQImage(mimeData->imageData().value<QImage>(), false)) {
                editor->replace(std::move(texture));
                editor->setFlipVertically(true);
                return;
            }
        }

    if (mimeData->hasText())
        if (auto editor = openNewSourceEditor(fileName)) {
            editor->replace(mimeData->text());
            return;
        }

    if (!mimeData->formats().isEmpty())
        if (auto editor = openNewBinaryEditor(fileName))
            editor->replace(mimeData->data(mimeData->formats()[0]));
}

void EditorManager::addSourceEditor(SourceEditor *editor)
{
    mSourceEditors.append(editor);

    auto dock = createDock(editor, editor);
    connect(editor, &SourceEditor::modificationChanged,
        dock, &QDockWidget::setWindowModified);
    connect(editor, &SourceEditor::fileNameChanged,
        [this, dock]() { handleEditorFilenameChanged(dock); });
    connect(editor, &SourceEditor::navigationPositionChanged,
        this, &EditorManager::addNavigationPosition);
}

void EditorManager::addTextureEditor(TextureEditor *editor)
{
    mTextureEditors.append(editor);

    auto dock = createDock(editor, editor);
    connect(editor, &TextureEditor::modificationChanged,
        dock, &QDockWidget::setWindowModified);
    connect(editor, &TextureEditor::fileNameChanged,
        [this, dock]() { handleEditorFilenameChanged(dock); });
}

void EditorManager::addBinaryEditor(BinaryEditor *editor)
{
    mBinaryEditors.append(editor);

    auto dock = createDock(editor, editor);
    connect(editor, &BinaryEditor::modificationChanged,
        dock, &QDockWidget::setWindowModified);
    connect(editor, &BinaryEditor::fileNameChanged,
        [this, dock]() { handleEditorFilenameChanged(dock); });
}

QDockWidget *EditorManager::findDockToAddTab(int tabifyGroup) 
{
    if (auto dock = mLastFocusedTabifyGroupDock[tabifyGroup])
        if (mDocks.count(dock))
            if (!dock->isFloating())
                return dock;

    for (auto [dock, editor] : mDocks)
        if (!dock->isFloating() && editor->tabifyGroup() == tabifyGroup)
            return dock;

    return nullptr;
}

QDockWidget *EditorManager::createDock(QWidget *widget, IEditor *editor)
{
    auto fileName = editor->fileName();
    auto dock = new QDockWidget(this);
    setDockWindowTitle(dock, fileName);
    dock->setWidget(widget);
    dock->setObjectName(QString::number(QRandomGenerator::global()->generate64(), 16));

    dock->setFeatures(QDockWidget::DockWidgetMovable |
        QDockWidget::DockWidgetClosable | 
        QDockWidget::DockWidgetFloatable);
    dock->toggleViewAction()->setVisible(false);
    dock->installEventFilter(this);

    if (const auto tabDock = findDockToAddTab(editor->tabifyGroup())) {
        tabifyDockWidget(tabDock, dock);
    }
    else {
        addDockWidget(Qt::TopDockWidgetArea, dock);
        resizeDocks({ dock }, { width() }, Qt::Horizontal);
    }
    mDocks.emplace(dock, editor);
    return dock;
}

void EditorManager::handleEditorFilenameChanged(QDockWidget *dock) 
{
    if (auto editor = mDocks[dock]) {
        setDockWindowTitle(dock, editor->fileName());
        Singletons::fileCache().handleEditorFileChanged(editor->fileName());
    }
}

void EditorManager::setDockWindowTitle(QDockWidget *dock, const QString &fileName) 
{
    dock->setWindowTitle(FileDialog::getWindowTitle(fileName));
    if (!FileDialog::isEmptyOrUntitled(fileName))
        dock->setStatusTip(fileName);
}

bool EditorManager::saveDock(QDockWidget *dock)
{
    auto currentDock = mCurrentDock;
    mCurrentDock = dock;
    auto result = saveEditor();
    mCurrentDock = currentDock;
    return result;
}

bool EditorManager::promptSaveDock(QDockWidget *dock)
{
    auto editor = mDocks[dock];
    if (dock->isWindowModified()) {
        autoRaise(dock->widget());

        const auto ret = showNotSavedDialog(this, editor->fileName());
        if (ret == QMessageBox::Cancel)
            return false;
        if (ret == QMessageBox::Save &&
            !saveDock(dock))
            return false;
    }
    return true;
}

void EditorManager::closeDock(QDockWidget *dock)
{
    auto editor = mDocks[dock];
    Q_EMIT editorRenamed(editor->fileName(), "");

    mSourceEditors.removeAll(static_cast<SourceEditor*>(editor));
    mBinaryEditors.removeAll(static_cast<BinaryEditor*>(editor));
    mTextureEditors.removeAll(static_cast<TextureEditor*>(editor));
    mQmlViews.removeAll(static_cast<QmlView*>(editor));

    mDocks.erase(dock);

    DockWindow::closeDock(dock);

    if (mCurrentDock == dock) {
        updateDockCurrentProperty(dock, false);
        mCurrentDock = nullptr;
        if (!mDocks.empty())
            autoRaise(mDocks.rbegin()->first->widget());
    }

    if (mDocks.empty())
        clearNavigationStack();
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

void EditorManager::clearNavigationStack()
{
    if (!mNavigationStackPosition)
        return;
    mNavigationStack.clear();
    mNavigationStackPosition = 0;
    Q_EMIT canNavigateForwardChanged(false);
    Q_EMIT canNavigateBackwardChanged(false);
}

void EditorManager::addNavigationPosition(const QString &position, bool update) 
{
    const auto couldNavigateForward = canNavigateForward();
    const auto couldNavigateBackward = canNavigateBackward();

    mNavigationStack.resize(mNavigationStackPosition);

    const auto editor = QObject::sender();
    if (update && !mNavigationStack.isEmpty() &&
          mNavigationStack.back().first == editor) {
        mNavigationStack.back().second = position;
    }
    else {
        mNavigationStack.push({ editor, position });
        ++mNavigationStackPosition;
    }

    if (canNavigateForward() != couldNavigateForward)
        Q_EMIT canNavigateForwardChanged(false);
    if (canNavigateBackward() != couldNavigateBackward)
        Q_EMIT canNavigateBackwardChanged(true);
}

bool EditorManager::restoreNavigationPosition(int index)
{
    // validate pointer, editor might have been closed
    const auto [object, position] = mNavigationStack[index];
    const auto editor = static_cast<SourceEditor*>(object);
    if (mSourceEditors.indexOf(editor) == -1)
        return false;

    editor->restoreNavigationPosition(position);
    autoRaise(editor);
    return true;
}

bool EditorManager::canNavigateBackward() const
{
    return (mNavigationStackPosition > 1);
}

bool EditorManager::canNavigateForward() const
{
    return (mNavigationStackPosition < mNavigationStack.size());
}

void EditorManager::navigateBackward()
{
    const auto couldNavigateBackward = canNavigateBackward();
    const auto couldNavigateForward = canNavigateForward();
    while (canNavigateBackward()) {
        --mNavigationStackPosition;
        if (restoreNavigationPosition(mNavigationStackPosition - 1))
            break;
    }
    if (canNavigateForward() != couldNavigateForward)
        Q_EMIT canNavigateForwardChanged(true);
    if (canNavigateBackward() != couldNavigateBackward)
        Q_EMIT canNavigateBackwardChanged(false);
}

void EditorManager::navigateForward()
{
    const auto couldNavigateForward = canNavigateForward();
    const auto couldNavigateBackward = canNavigateBackward();
    while (canNavigateForward()) {
        ++mNavigationStackPosition;
        if (restoreNavigationPosition(mNavigationStackPosition - 1))
            break;
    }
    if (canNavigateForward() != couldNavigateForward)
        Q_EMIT canNavigateForwardChanged(false);
    if (canNavigateBackward() != couldNavigateBackward)
        Q_EMIT canNavigateBackwardChanged(true);
}
