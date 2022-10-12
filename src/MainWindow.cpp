#include "MainWindow.h"
#include "ui_MainWindow.h"
#include "AutoOrientationSplitter.h"
#include "session/SessionEditor.h"
#include "session/SessionProperties.h"
#include "session/SessionModel.h"
#include "Singletons.h"
#include "AboutDialog.h"
#include "MessageWindow.h"
#include "OutputWindow.h"
#include "MessageList.h"
#include "Settings.h"
#include "SynchronizeLogic.h"
#include "InputState.h"
#include "editors/EditorManager.h"
#include "scripting/CustomActions.h"
#include <QCloseEvent>
#include <QMessageBox>
#include <QDockWidget>
#include <QDesktopServices>
#include <QActionGroup>
#include <QMenu>
#include <QToolButton>
#include <QCoreApplication>
#include <QTimer>
#include <QMimeData>
#include <QScreen>

void setFileDialogDirectory(const QString &fileName)
{
    if (!FileDialog::isEmptyOrUntitled(fileName))
        Singletons::fileDialog().setDirectory(QFileInfo(fileName).dir());
}

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , mUi(new Ui::MainWindow)
    , mMessageWindow(new MessageWindow())
    , mCustomActions(new CustomActions(this))
    , mSingletons(new Singletons(this))
    , mOutputWindow(new OutputWindow())
    , mEditorManager(Singletons::editorManager())
    , mSessionEditor(new SessionEditor())
    , mSessionProperties(new SessionProperties())
{
    mUi->setupUi(this);

    setAcceptDrops(true);

    auto icon = QIcon(":images/16x16/icon.png");
    icon.addFile(":images/32x32/icon.png");
    icon.addFile(":images/64x64/icon.png");
    setWindowIcon(icon);

    mUi->toolBarMain->setIconSize(QSize(20, 20));
    mUi->toolBarMain->toggleViewAction()->setVisible(false);
#if defined(_WIN32)
    mUi->toolBarMain->setContentsMargins(0, 0, 0, 4);
#endif

    takeCentralWidget();

    auto content = new QWidget(this);
    mEditorManager.setParent(content);
    auto layout = new QVBoxLayout(content);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);
    layout->addWidget(&mEditorManager);
    layout->addWidget(mEditorManager.createEditorPropertiesPanel(mUi->actionFindReplace));

    mFullScreenBar = new QToolBar(this);
    mFullScreenTitle = new QLabel(this);
    auto fullScreenCloseButton = new QToolButton(this);
    fullScreenCloseButton->setDefaultAction(mUi->actionFullScreenClose);
    fullScreenCloseButton->setMaximumSize(24, 24);
    mFullScreenBar->setVisible(false);
    mFullScreenBar->addWidget(mFullScreenTitle);
    mFullScreenBar->addWidget(fullScreenCloseButton);
    mFullScreenBar->setStyleSheet("* { margin:0 }");
    mUi->menubar->setCornerWidget(mFullScreenBar);

    mEditorManager.createEditorToolBars(mUi->toolBarMain);

    auto menuHamburger = new QMenu(this);
    for (auto action : mUi->menuFile->actions()) 
        menuHamburger->addAction(action);
    menuHamburger->insertSeparator(mUi->actionQuit);
    menuHamburger->insertMenu(mUi->actionQuit, mUi->menuEdit);
    menuHamburger->insertMenu(mUi->actionQuit, mUi->menuSession);
    menuHamburger->insertMenu(mUi->actionQuit, mUi->menuView);
    menuHamburger->insertMenu(mUi->actionQuit, mUi->menuHelp);
    menuHamburger->insertSeparator(mUi->actionQuit);

    mMenuButton = new QToolButton(this);
    mMenuButton->setVisible(false);
    mMenuButton->setMenu(menuHamburger);
    mMenuButton->setStyleSheet("*::menu-indicator { image: none; }");
    mMenuButton->setArrowType(Qt::NoArrow);
    mMenuButton->setText("Menu");
    mMenuButton->setToolTip("Menu");
    mMenuButton->setIcon(QIcon(":images/16x16/hamburger.png"));
    mMenuButton->setPopupMode(QToolButton::InstantPopup);

    auto dock = new QDockWidget(this);
    dock->setWidget(content);
    dock->setObjectName("Editors");
    dock->setFeatures(QDockWidget::NoDockWidgetFeatures);
    dock->setTitleBarWidget(new QWidget(this));
    dock->toggleViewAction()->setVisible(false);
    addDockWidget(Qt::RightDockWidgetArea, dock);

    mSessionSplitter = new AutoOrientationSplitter(this);
    mSessionSplitter->addWidget(mSessionEditor.data());
    mSessionSplitter->addWidget(mSessionProperties.data());

    dock = new QDockWidget(tr("Session"), this);
    dock->setObjectName("Session");
    dock->setFeatures(QDockWidget::DockWidgetClosable |
                      QDockWidget::DockWidgetMovable);
    dock->setWidget(mSessionSplitter);
    dock->setVisible(false);
    auto action = dock->toggleViewAction();
    action->setText(tr("Show &") + action->text());
    action->setIcon(QIcon(":images/16x16/format-indent-more.png"));
    mUi->menuView->addAction(action);
    mUi->toolBarMain->insertAction(mUi->actionEvalReset, action);
    addDockWidget(Qt::LeftDockWidgetArea, dock);
    mSessionEditor->addItemActions(mUi->menuSession);
    mSessionDock = dock;

    dock = new QDockWidget(tr("Messages"), this);
    dock->setObjectName("Messages");
    dock->setFeatures(QDockWidget::DockWidgetClosable |
                      QDockWidget::DockWidgetMovable);
    dock->setWidget(mMessageWindow.data());
    dock->setVisible(false);
    action = dock->toggleViewAction();
    action->setText(tr("Show &") + action->text());
    action->setIcon(QIcon(":images/16x16/help-faq.png"));
    mUi->menuView->addAction(action);
    mUi->toolBarMain->insertAction(mUi->actionEvalReset, action);
    addDockWidget(Qt::RightDockWidgetArea, dock);

    dock = new QDockWidget(tr("Output"), this);
    dock->setObjectName("Output");
    dock->setFeatures(QDockWidget::DockWidgetClosable |
                      QDockWidget::DockWidgetMovable |
                      QDockWidget::DockWidgetFloatable);
    dock->setWidget(mOutputWindow.data());
    dock->setVisible(false);
    action = dock->toggleViewAction();
    action->setText(tr("Show &") + action->text());
    action->setIcon(QIcon(":images/16x16/utilities-terminal.png"));
    mUi->menuView->addAction(action);
    mUi->toolBarMain->insertAction(mUi->actionEvalReset, action);
    addDockWidget(Qt::RightDockWidgetArea, dock);
    auto outputDock = dock;

    mUi->toolBarMain->insertSeparator(mUi->actionEvalReset);

    mUi->actionQuit->setShortcuts(QKeySequence::Quit);
    mUi->actionNew->setShortcuts(QKeySequence::New);
    mUi->actionOpen->setShortcuts(QKeySequence::Open);
    mUi->actionSave->setShortcuts(QKeySequence::Save);
    mUi->actionSaveAs->setShortcuts(QKeySequence::SaveAs);
    mUi->actionClose->setShortcuts(QKeySequence::Close);
    mUi->actionUndo->setShortcuts(QKeySequence::Undo);
    mUi->actionRedo->setShortcuts(QKeySequence::Redo);
    mUi->actionCut->setShortcuts(QKeySequence::Cut);
    mUi->actionCopy->setShortcuts(QKeySequence::Copy);
    mUi->actionPaste->setShortcuts(QKeySequence::Paste);
    mUi->actionDelete->setShortcuts(QKeySequence::Delete);
    mUi->actionSelectAll->setShortcuts(QKeySequence::SelectAll);
    mUi->actionOnlineHelp->setShortcuts(QKeySequence::HelpContents);
    mUi->actionFindReplace->setShortcuts({ QKeySequence::Find, QKeySequence::Replace });

    addAction(mUi->actionFocusNextEditor);
    addAction(mUi->actionFocusPreviousEditor);

    auto windowFileName = new QAction(this);
    mEditActions = EditActions{
        windowFileName,
        mUi->actionUndo,
        mUi->actionRedo,
        mUi->actionCut,
        mUi->actionCopy,
        mUi->actionPaste,
        mUi->actionDelete,
        mUi->actionSelectAll,
        mUi->actionRename,
        mUi->actionFindReplace
    };

    connect(mUi->actionNew, &QAction::triggered,
        this, &MainWindow::newFile);
    connect(mUi->actionOpen, &QAction::triggered,
        this, static_cast<void(MainWindow::*)()>(&MainWindow::openFile));
    connect(mUi->actionReload, &QAction::triggered,
        this, &MainWindow::reloadFile);
    connect(mUi->actionSave, &QAction::triggered,
        this, &MainWindow::saveFile);
    connect(mUi->actionSaveAs, &QAction::triggered,
        this, &MainWindow::saveFileAs);
    connect(mUi->actionSaveAll, &QAction::triggered,
        this, &MainWindow::saveAllFiles);
    connect(mUi->actionClose, &QAction::triggered,
        this, &MainWindow::closeFile);
    connect(mUi->actionCloseAll, &QAction::triggered,
        this, &MainWindow::closeAllFiles);
    connect(mUi->actionQuit, &QAction::triggered,
        this, &MainWindow::close);
    connect(mUi->actionFullScreenClose, &QAction::triggered,
        this, &MainWindow::close);
    connect(mUi->actionOpenContainingFolder, &QAction::triggered,
        this, &MainWindow::openContainingFolder);
    connect(mUi->actionOnlineHelp, &QAction::triggered,
        this, &MainWindow::openOnlineHelp);
    connect(mUi->menuHelp, &QMenu::aboutToShow,
        this, &MainWindow::populateSampleSessions);
    connect(mUi->menuRecentFiles, &QMenu::aboutToShow,
        this, &MainWindow::updateRecentFileActions);
    connect(mUi->actionAbout, &QAction::triggered,
        this, &MainWindow::openAbout);
    connect(windowFileName, &QAction::changed,
        this, &MainWindow::updateFileActions);
    connect(mUi->actionFocusNextEditor, &QAction::triggered,
        this, &MainWindow::focusNextEditor);
    connect(mUi->actionFocusPreviousEditor, &QAction::triggered,
        this, &MainWindow::focusPreviousEditor);
    connect(mUi->actionNavigateBackward, &QAction::triggered,
        &mEditorManager, &EditorManager::navigateBackward);
    connect(mUi->actionNavigateForward, &QAction::triggered,
        &mEditorManager, &EditorManager::navigateForward);
    connect(&mEditorManager, &EditorManager::canNavigateBackwardChanged,
        mUi->actionNavigateBackward, &QAction::setEnabled);
    connect(&mEditorManager, &EditorManager::canNavigateForwardChanged,
        mUi->actionNavigateForward, &QAction::setEnabled);
    connect(&mEditorManager, &EditorManager::canPasteInNewEditorChanged,
        mUi->actionPasteInNewEditor, &QAction::setEnabled);
    connect(mUi->actionPasteInNewEditor, &QAction::triggered,
        &mEditorManager, &EditorManager::pasteInNewEditor);
    connect(qApp, &QApplication::focusChanged,
        this, &MainWindow::updateCurrentEditor);
    connect(mUi->actionFullScreen, &QAction::triggered,
        this, &MainWindow::setFullScreen);
    connect(mSessionEditor->selectionModel(), &QItemSelectionModel::currentChanged,
        mSessionProperties.data(), &SessionProperties::setCurrentModelIndex);
    connect(mSessionEditor.data(), &SessionEditor::itemAdded,
        this, &MainWindow::openSessionDock);
    connect(mSessionEditor.data(), &SessionEditor::itemActivated,
        mSessionProperties.data(), &SessionProperties::openItemEditor);
    connect(mUi->menuSession, &QMenu::aboutToShow,
        mSessionEditor.data(), &SessionEditor::updateItemActions);
    connect(&mEditorManager, &DockWindow::openNewDock,
        this, &MainWindow::newFile);

    auto &synchronizeLogic = Singletons::synchronizeLogic();
    connect(mOutputWindow.data(), &OutputWindow::typeSelectionChanged,
        &synchronizeLogic, &SynchronizeLogic::setProcessSourceType);
    connect(outputDock, &QDockWidget::visibilityChanged,
        [this](bool visible) {
            Singletons::synchronizeLogic().setProcessSourceType(
                visible ? mOutputWindow->selectedType() : "");
        });
    connect(mMessageWindow.data(), &MessageWindow::messageActivated,
        this, &MainWindow::handleMessageActivated);
    connect(mMessageWindow.data(), &MessageWindow::messagesAdded,
        this, &MainWindow::openMessageDock);
    connect(&synchronizeLogic, &SynchronizeLogic::outputChanged,
        mOutputWindow.data(), &OutputWindow::setText);
    connect(&Singletons::inputState(), &InputState::mouseChanged, 
        &synchronizeLogic, &SynchronizeLogic::handleMouseStateChanged);
    connect(&Singletons::inputState(), &InputState::keysChanged, 
        &synchronizeLogic, &SynchronizeLogic::handleKeyboardStateChanged);

    auto &settings = Singletons::settings();
    connect(mUi->actionSelectFont, &QAction::triggered,
        &settings, &Settings::selectFont);
    connect(mUi->actionShowWhiteSpace, &QAction::toggled,
        &settings, &Settings::setShowWhiteSpace);
    connect(mUi->actionDarkTheme, &QAction::toggled,
        &settings, &Settings::setDarkTheme);
    connect(mUi->actionHideMenuBar, &QAction::toggled,
        &settings, &Settings::setHideMenuBar);
    connect(mUi->actionLineWrapping, &QAction::toggled,
        &settings, &Settings::setLineWrap);
    connect(mUi->actionIndentWithSpaces, &QAction::toggled,
        &settings, &Settings::setIndentWithSpaces);
    connect(&settings, &Settings::darkThemeChanging,
        this, &MainWindow::handleDarkThemeChanging);
    connect(&settings, &Settings::hideMenuBarChanged,
        this, &MainWindow::handleHideMenuBarChanged);

    connect(mUi->actionEvalReset, &QAction::triggered,
        this, &MainWindow::updateEvaluationMode);
    connect(mUi->actionEvalManual, &QAction::triggered,
        this, &MainWindow::updateEvaluationMode);
    connect(mUi->actionEvalAuto, &QAction::toggled,
        this, &MainWindow::updateEvaluationMode);
    connect(mUi->actionEvalSteady, &QAction::toggled,
        this, &MainWindow::updateEvaluationMode);

    connect(mUi->menuCustomActions, &QMenu::aboutToShow,
        this, &MainWindow::updateCustomActionsMenu);
    connect(mUi->actionManageCustomActions, &QAction::triggered,
        mCustomActions.data(), &QDialog::show);

    qApp->installEventFilter(this);

    mUi->actionPasteInNewEditor->setEnabled(mEditorManager.canPasteInNewEditor());

    auto indentActionGroup = new QActionGroup(this);
    connect(indentActionGroup, &QActionGroup::triggered,
        [](QAction* a) { Singletons::settings().setTabSize(a->text().toInt()); });
    for (auto i = 1; i <= 8; i++) {
        auto action = mUi->menuTabSize->addAction(QString::number(i));
        action->setCheckable(true);
        action->setChecked(i == settings.tabSize());
        action->setActionGroup(indentActionGroup);
    }

    for (auto i = 0; i < 9; ++i) {
        auto action = mUi->menuRecentFiles->addAction("");
        connect(action, &QAction::triggered,
            this, &MainWindow::openRecentFile);
        mRecentSessionActions += action;
    }

    mUi->menuRecentFiles->addSeparator();

    for (auto i = 0; i < 26; ++i) {
        auto action = mUi->menuRecentFiles->addAction("");
        connect(action, &QAction::triggered,
            this, &MainWindow::openRecentFile);
        mRecentFileActions += action;
    }

    readSettings();
}

MainWindow::~MainWindow()
{
    disconnect(qApp, &QApplication::focusChanged,
        this, &MainWindow::updateCurrentEditor);

    mSingletons.reset();
    delete mUi;
}

void MainWindow::writeSettings()
{
    auto &settings = Singletons::settings();
    if (!isMaximized() && !isFullScreen())
        settings.setValue("geometry", saveGeometry());
    if (!isFullScreen())
        settings.setValue("maximized", isMaximized());
    settings.setValue("fullScreen", isFullScreen());
    settings.setValue("state", saveState());
    settings.setValue("sessionSplitter", mSessionSplitter->saveState());

    auto &fileDialog = Singletons::fileDialog();
    settings.setValue("lastDirectory", fileDialog.directory().absolutePath());

    settings.setValue("recentFiles", mRecentFiles);
}

void MainWindow::readSettings()
{
    const auto &settings = Singletons::settings();
    if (!restoreGeometry(settings.value("geometry").toByteArray()))
        setGeometry(100, 100, 800, 600);
    if (settings.value("fullScreen").toBool())
        setFullScreen(true);
    else if (settings.value("maximized").toBool())
        showMaximized();

    // workaround: restore state after geometry is applied, so it is not garbled
    QTimer::singleShot(1, this, [this]() {
        const auto &settings = Singletons::settings();
        restoreState(settings.value("state").toByteArray());
        mSessionSplitter->restoreState(settings.value("sessionSplitter").toByteArray());
    });

    Singletons::fileDialog().setDirectory(settings.value("lastDirectory").toString());

    mRecentFiles = settings.value("recentFiles").toStringList();
    mUi->actionIndentWithSpaces->setChecked(settings.indentWithSpaces());
    mUi->actionShowWhiteSpace->setChecked(settings.showWhiteSpace());
    mUi->actionDarkTheme->setChecked(settings.darkTheme());
    mUi->actionHideMenuBar->setChecked(settings.hideMenuBar());
    mUi->actionLineWrapping->setChecked(settings.lineWrap());
    mUi->actionFullScreen->setChecked(isFullScreen());
    handleDarkThemeChanging(settings.darkTheme());
    if (settings.hideMenuBar())
       handleHideMenuBarChanged(true);
}

void MainWindow::dragEnterEvent(QDragEnterEvent *event)
{
    if (event->mimeData()->hasUrls()) {
        event->acceptProposedAction();
    }
}

void MainWindow::dropEvent(QDropEvent *event)
{
    const auto urls = event->mimeData()->urls();
    for (const QUrl &url : urls)
        openFile(url.toLocalFile());

    setWindowState(Qt::WindowState::WindowActive);
    raise();
    activateWindow();
}

bool MainWindow::eventFilter(QObject *watched, QEvent *event)
{
    switch (event->type()) {
        case QEvent::KeyPress:
          mLastPressWasAlt = false;
          break;
     }
     return QMainWindow::eventFilter(watched, event);
}

void MainWindow::keyPressEvent(QKeyEvent *event) 
{
    if (event->key() == Qt::Key_Alt)
        mLastPressWasAlt = true;

    QMainWindow::keyPressEvent(event);
}

void MainWindow::keyReleaseEvent(QKeyEvent *event) 
{
    if (event->key() == Qt::Key_Alt && mLastPressWasAlt)
        if (!std::exchange(mIgnoreNextAlt, false))
            return mMenuButton->click();
    
    QMainWindow::keyReleaseEvent(event);
}

void MainWindow::closeEvent(QCloseEvent *event)
{
    if (!closeSession()) {
        event->ignore();
        return;
    }
    event->accept();
    writeSettings();
}

void MainWindow::focusNextEditor()
{
    if (!mEditorManager.focusNextEditor())
        mSessionEditor->setFocus();
}

void MainWindow::focusPreviousEditor()
{
    if (!mEditorManager.focusPreviousEditor())
        mSessionEditor->setFocus();
}

void MainWindow::setFullScreen(bool fullScreen)
{
    if (fullScreen) {
        setContentsMargins(0, 0, 0, 0);
        showFullScreen();
        mFullScreenBar->show();
        updateFileActions();
    }
    else {
        setContentsMargins(1, 1, 1, 1);
        mFullScreenBar->hide();
        if (mSingletons->settings().value("maximized").toBool()) {
            showMaximized();
        }
        else {
            showNormal();
        }
    }
}

void MainWindow::updateCurrentEditor()
{
    // do not switch, when there are both
    // session and editors, but none is focused
    auto focusWidget = qApp->focusWidget();
    if (mEditorManager.hasEditor() &&
        !mEditorManager.isAncestorOf(focusWidget) &&
        !mSessionSplitter->isAncestorOf(focusWidget))
        return;

    mEditorManager.updateCurrentEditor();
    disconnectEditActions();
    connectEditActions();
    updateDockCurrentProperty(mSessionDock, !mEditorManager.hasCurrentEditor());
    setFileDialogDirectory(mEditorManager.hasCurrentEditor() ? 
        mEditorManager.currentEditorFileName() : mSessionEditor->fileName());
}

void MainWindow::disconnectEditActions()
{
    for (const auto &connection : qAsConst(mConnectedEditActions))
        disconnect(connection);
    mConnectedEditActions.clear();

    auto actions = {
        mEditActions.undo, mEditActions.redo, mEditActions.cut,
        mEditActions.copy, mEditActions.paste, mEditActions.delete_,
        mEditActions.selectAll, mEditActions.rename, mEditActions.findReplace
    };
    for (auto action : actions)
        action->setEnabled(false);
}

void MainWindow::connectEditActions()
{
    mConnectedEditActions = mEditorManager.hasCurrentEditor() ?
        mEditorManager.connectEditActions(mEditActions) :
        mSessionEditor->connectEditActions(mEditActions);
}

void MainWindow::updateFileActions()
{
    const auto fileName = mEditActions.windowFileName->text();
    const auto modified = mEditActions.windowFileName->isEnabled();
    auto title = QString((modified ? "*" : "") +
        FileDialog::getFullWindowTitle(fileName) +
        " - " + qApp->applicationName());
    setWindowTitle(title);

    if (isFullScreen()) {
        title.replace("[*]", "");
        mFullScreenTitle->setText(title + "  ");
        mFullScreenBar->hide();
        mFullScreenBar->show();
    }

    const auto desc = QString(" \"%1\"").arg(FileDialog::getFileTitle(fileName));
    mUi->actionSave->setText(tr("&Save%1").arg(desc));
    mUi->actionSaveAs->setText(tr("Save%1 &As...").arg(desc));
    mUi->actionClose->setText(tr("&Close%1").arg(desc));

    const auto hasFile = !FileDialog::isEmptyOrUntitled(fileName);
    mUi->actionReload->setEnabled(hasFile);
    mUi->actionReload->setText(tr("&Reload%1").arg(hasFile ? desc : ""));
    mUi->actionOpenContainingFolder->setEnabled(hasFile);
}

void MainWindow::stopEvaluation()
{
    mUi->actionEvalAuto->setChecked(false);
    mUi->actionEvalSteady->setChecked(false);
    Singletons::synchronizeLogic().resetRenderSession();
}

void MainWindow::updateEvaluationMode()
{
    if (QObject::sender() == mUi->actionEvalReset) {
        Singletons::synchronizeLogic().resetEvaluation();
    }
    else if (QObject::sender() == mUi->actionEvalManual) {
        Singletons::synchronizeLogic().manualEvaluation();
    }
    else if (QObject::sender() == mUi->actionEvalAuto) {
        if (mUi->actionEvalAuto->isChecked())
            mUi->actionEvalSteady->setChecked(false);
    }
    else {
        if (mUi->actionEvalSteady->isChecked())
            mUi->actionEvalAuto->setChecked(false);
    }

    Singletons::synchronizeLogic().setEvaluationMode(
        mUi->actionEvalAuto->isChecked() ? EvaluationMode::Automatic :
        mUi->actionEvalSteady->isChecked() ? EvaluationMode::Steady :
        EvaluationMode::Paused);
}

bool MainWindow::hasEditor() const
{
    return mEditorManager.hasEditor();
}

void MainWindow::newFile()
{
    mEditorManager.openNewSourceEditor(
        FileDialog::generateNextUntitledFileName(tr("Untitled")));
}

void MainWindow::openFile()
{
    auto options = FileDialog::Options{
        FileDialog::Multiselect |
        FileDialog::AllExtensionFilters
    };
    auto& dialog = Singletons::fileDialog();
    if (dialog.exec(options)) {
        const auto fileNames = dialog.fileNames();
        for (const QString &fileName : fileNames)
            openFile(fileName, dialog.asBinaryFile());
    }
}

bool MainWindow::openFile(const QString &fileName,
    bool asBinaryFile)
{
    if (FileDialog::isSessionFileName(fileName)) {
        if (!openSession(fileName))
            return false;
    }
    else {
        if (!mEditorManager.openEditor(fileName, asBinaryFile))
            return false;
    }
    addToRecentFileList(fileName);
    return true;
}

bool MainWindow::saveFile()
{
    if (!mEditorManager.hasCurrentEditor()) {
        if (!saveSession())
            return false;
        addToRecentFileList(mSessionEditor->fileName());
    }
    else {
        if (!mEditorManager.saveEditor())
            return false;
        addToRecentFileList(mEditorManager.currentEditorFileName());
    }
    return true;
}

bool MainWindow::saveFileAs()
{
    if (!mEditorManager.hasCurrentEditor()) {
        if (!saveSessionAs())
            return false;
        addToRecentFileList(mSessionEditor->fileName());
    }
    else {
        if (!mEditorManager.saveEditorAs())
            return false;
        addToRecentFileList(mEditorManager.currentEditorFileName());
    }
    return true;
}

bool MainWindow::saveAllFiles()
{
    if (!mEditorManager.saveAllEditors())
        return false;
    if (!mSessionEditor->isModified())
        return true;
    return saveSession();
}

bool MainWindow::reloadFile()
{
    if (mEditorManager.hasCurrentEditor())
        return mEditorManager.reloadEditor();

    return reloadSession();
}

bool MainWindow::closeFile()
{
    if (mEditorManager.hasCurrentEditor())
        return mEditorManager.closeEditor();

    return closeSession();
}

bool MainWindow::closeAllFiles()
{
    if (!mEditorManager.closeAllEditors())
        return false;

    updateCurrentEditor();
    return true;
}

bool MainWindow::openSession(const QString &fileName)
{
    if (!closeSession())
        return false;

    mSessionEditor->setFileName(fileName);
    if (!mSessionEditor->load())
        return false;

    if (!restoreSessionState(fileName)) {
        openSessionDock();
        auto &editors = Singletons::editorManager();
        editors.setAutoRaise(false);
        mSessionEditor->activateFirstItem();
        editors.setAutoRaise(true);
    }
    return true;
}

bool MainWindow::saveSession()
{
    if (FileDialog::isUntitled(mSessionEditor->fileName()) ||
        !mSessionEditor->save())
        return saveSessionAs();

    saveSessionState(mSessionEditor->fileName());
    addToRecentFileList(mSessionEditor->fileName());
    return true;
}

bool MainWindow::saveSessionAs()
{
    auto options = FileDialog::Options{
        FileDialog::Saving |
        FileDialog::SessionExtensions
    };
    const auto prevFileName = mSessionEditor->fileName();
    for (;;) {
        if (!Singletons::fileDialog().exec(options, mSessionEditor->fileName())) {
            mSessionEditor->setFileName(prevFileName);
            return false;
        }
        mSessionEditor->setFileName(Singletons::fileDialog().fileName());
        if (mSessionEditor->save())
            break;
    }

    if (!copySessionFiles(QFileInfo(prevFileName).path(), 
            QFileInfo(mSessionEditor->fileName()).path())) {
        QMessageBox dialog(this);
        dialog.setIcon(QMessageBox::Warning);
        dialog.setWindowTitle(tr("File Error"));
        dialog.setText(tr("Copying session files failed."));
        dialog.addButton(QMessageBox::Ok);
        dialog.exec();
    }

    mSessionEditor->save();
    mSessionEditor->clearUndo();
    saveSessionState(mSessionEditor->fileName());
    addToRecentFileList(mSessionEditor->fileName());
    return true;
}

bool MainWindow::copySessionFiles(const QString &fromPath, const QString &toPath) 
{
    const auto startsWithPath = [](const QString& string, const QString& start) {
        if (start.length() > string.length())
            return false;
        const auto isSlash = [](QChar c) { return (c == '/' || c == '\\'); };
        for (auto i = 0; i < start.length(); ++i)
            if (start[i] != string[i] && (!isSlash(string[i]) || !isSlash(start[i])))
                return false;
        return true;
    };

    auto succeeded = true;
    auto &model = Singletons::sessionModel();
    model.forEachFileItem([&](const FileItem &fileItem) {
        const auto prevFileName = fileItem.fileName;
        if (startsWithPath(prevFileName, fromPath)) {
            const auto newFileName = QString(toPath + prevFileName.mid(fromPath.length()));
            if (QFileInfo::exists(newFileName))
                return;

            QDir().mkpath(QFileInfo(newFileName).path());
            if (!QFile(prevFileName).copy(newFileName)) {
                succeeded = false;
                return;
            }
            model.setData(model.getIndex(&fileItem, SessionModel::FileName), newFileName);

            // rename editor filenames
            Singletons::editorManager().renameEditors(prevFileName, newFileName);
        }
    });
    return succeeded;
}

void MainWindow::saveSessionState(const QString &sessionFileName)
{
    const auto sessionStateFile = QString(sessionFileName + ".state");
    auto settings = QSettings(sessionStateFile, QSettings::IniFormat);
    auto openEditors = QStringList();
    mSingletons->sessionModel().forEachFileItem([&](const FileItem& item) {
        if (auto editor = mEditorManager.getEditor(item.fileName))
            openEditors += QString("%1|%2").arg(item.id)
                .arg(mEditorManager.getEditorObjectName(editor));
    });
    settings.setValue("editorState", mEditorManager.saveState());
    settings.setValue("openEditors", openEditors);
    auto &synchronizeLogic = Singletons::synchronizeLogic();
    settings.setValue("shaderPreamble", synchronizeLogic.sessionShaderPreamble());
    settings.setValue("shaderIncludePaths", synchronizeLogic.sessionShaderIncludePaths());
}

bool MainWindow::restoreSessionState(const QString &sessionFileName)
{
    const auto sessionStateFile = QString(sessionFileName + ".state");
    if (!QFileInfo::exists(sessionStateFile))
        return false;

    auto &model = Singletons::sessionModel();
    auto settings = QSettings(sessionStateFile, QSettings::IniFormat);
    const auto openEditors = settings.value("openEditors").toStringList();
    mEditorManager.setAutoRaise(false);
    for (const auto &openEditor : openEditors)
        if (const auto sep = openEditor.indexOf('|'); sep > 0) {
            const auto itemId = openEditor.midRef(0, sep).toInt();
            const auto editorObjectName = openEditor.mid(sep + 1);
            if (auto item = model.findItem(itemId))
                if (auto editor = mSessionProperties->openItemEditor(model.getIndex(item)))
                    mEditorManager.setEditorObjectName(editor, editorObjectName);
        }
    mEditorManager.restoreState(settings.value("editorState").toByteArray());   
    mEditorManager.setAutoRaise(true);

    auto &synchronizeLogic = Singletons::synchronizeLogic();
    synchronizeLogic.setSessionShaderPreamble(
        settings.value("shaderPreamble").toString());
    synchronizeLogic.setSessionShaderIncludePaths(
        settings.value("shaderIncludePaths").toString());
    return true;
}

bool MainWindow::closeSession()
{
    stopEvaluation();

    if (!mEditorManager.promptSaveAllEditors())
        return false;

    if (mSessionEditor->isModified()) {
        auto ret = openNotSavedDialog(this, mSessionEditor->fileName());
        if (ret == QMessageBox::Cancel)
            return false;

        if (ret == QMessageBox::Save &&
            !saveSession())
            return false;
    }

    mEditorManager.closeAllEditors(false);
    updateCurrentEditor();

    return mSessionEditor->clear();
}

bool MainWindow::reloadSession()
{
    return openFile(mSessionEditor->fileName());
}

void MainWindow::addToRecentFileList(QString fileName)
{
    fileName = QDir::toNativeSeparators(fileName);
    mRecentFiles.removeAll(fileName);
    mRecentFiles.prepend(fileName);
    setFileDialogDirectory(fileName);
}

void MainWindow::updateRecentFileActions()
{
    auto recentFileIndex = 0;
    auto recentSessionIndex = 0;
    QMutableStringListIterator i(mRecentFiles);
    while (i.hasNext()) {
        const auto filename = i.next();
        auto action = std::add_pointer_t<QAction>();
        if (FileDialog::isSessionFileName(filename)) {
            if (recentSessionIndex < mRecentSessionActions.size())
                action = mRecentSessionActions[recentSessionIndex++];
        }
        else {
            if (recentFileIndex < mRecentFileActions.size())
                action = mRecentFileActions[recentFileIndex++];
        }
        if (action) {
            action->setText(filename);
            action->setData(filename);
            if (!filename.startsWith("\\"))
                action->setEnabled(QFile::exists(filename));
            action->setVisible(true);
        }
        else {
            i.remove();
        }
    }
    for (auto i = recentSessionIndex; i < mRecentSessionActions.size(); ++i)
        mRecentSessionActions[i]->setVisible(false);
    for (auto i = recentFileIndex; i < mRecentFileActions.size(); ++i)
        mRecentFileActions[i]->setVisible(false);

    auto index = 0;
    for (const auto &actions : { mRecentSessionActions, mRecentFileActions })
        for (const auto action : actions)
            if (action->isVisible()) {
                action->setText(QStringLiteral("  &%1 %2").arg(
                    QChar(index < 9 ? '1' + index : 'A' + (index - 9))).arg(action->text()));
                ++index;
            }
}

void MainWindow::openRecentFile()
{
    if (auto action = qobject_cast<QAction *>(sender()))
        openFile(action->data().toString());
}

void MainWindow::updateCustomActionsMenu()
{
    auto &model = Singletons::sessionModel();
    auto selection = mSessionEditor->selectionModel()->selectedIndexes();
    mCustomActions->setSelection(model.getJson(selection));

    mUi->menuCustomActions->clear();
    mUi->menuCustomActions->addActions(mCustomActions->getApplicableActions());
    mUi->menuCustomActions->addSeparator();
    mUi->menuCustomActions->addAction(mUi->actionManageCustomActions);
}

void MainWindow::handleMessageActivated(ItemId itemId, QString fileName,
        int line, int column)
{
    if (itemId) {
        mSessionEditor->setCurrentItem(itemId);
        openSessionDock();
    }
    else if (!fileName.isEmpty()) {
        Singletons::editorManager().openSourceEditor(
            fileName, line, column);
    }
}

void MainWindow::handleDarkThemeChanging(bool darkTheme)
{
#if defined(_WIN32)
    const auto inWindows = true;
#else
    const auto inWindows = false;
#endif

    auto frameDarker = 105;
    auto currentFrameDarker = 120;
    auto palette = qApp->style()->standardPalette();
    if (darkTheme) {
        struct S { QPalette::ColorRole role; QColor a; QColor i; QColor d; };
        const auto colors = std::initializer_list<S>{
            { QPalette::WindowText, 0xCFCFCF, 0xCFCFCF, 0x6A6A6A },
            { QPalette::Button, 0x252525, 0x252525, 0x252525 },
            { QPalette::Light, 0x4B4B51, 0x4B4B51, 0x111111 },
            { QPalette::Midlight, 0xCBCBCB, 0xCBCBCB, 0xCBCBCB },
            { QPalette::Dark, 0x9F9F9F, 0x9F9F9F, 0xBEBEBE },
            { QPalette::Mid, 0xB8B8B8, 0xB8B8B8, 0xB8B8B8 },
            { QPalette::Text, 0xCFCFCF, 0xCFCFCF, 0x8F8F8F },
            { QPalette::ButtonText, 0xCFCFCF, 0xCFCFCF, 0x8B8B8B },
            { QPalette::Base, 0x232323, 0x232323, 0x2A2A2A },
            { QPalette::Window, 0x252525, 0x252525, 0x252525 },
            { QPalette::Shadow, 0x767472, 0x767472, 0x767472 },
            { QPalette::Highlight, 0x405D86, 0x405D86, 0x343434 },
            { QPalette::Link, 0xFFFFFF, 0xFFFFFF, 0xFFFFFF },
            { QPalette::AlternateBase, 0x212121, 0x212121, 0x212121 },
            { QPalette::ToolTipBase, 0x2F2F2F, 0x2F2F2F, 0x45454B },
            { QPalette::ToolTipText, 0xCACACA, 0xCACACA, 0x8A8A8A },
            { QPalette::PlaceholderText, 0xCFCFCF, 0xCFCFCF, 0xCFCFCF },
        };
        for (const auto &s : colors) {
            palette.setColor(QPalette::Active, s.role, s.a);
            palette.setColor(QPalette::Inactive, s.role, s.i);
            palette.setColor(QPalette::Disabled, s.role, s.d);
        }
        frameDarker = 90;
        currentFrameDarker = 70;
    }

    qApp->setPalette(palette);

    const auto color = [&](QPalette::ColorRole role,
          QPalette::ColorGroup group, int darker = 100) {
        return palette.brush(group, role).color().darker(darker).name(QColor::HexRgb);
    };
    auto styleSheet = QString(
      "QLabel:disabled { color: %1 }\n"
      "QDockWidget > QFrame { border:2px solid %2 }\n"
      "QDockWidget[current=true] > QFrame { border:2px solid %3 }\n"
      "QMenuBar { background-color: %4; padding-top:2px }\n"
      "QToolBar { background-color: %4 }\n")
      .arg(color(QPalette::WindowText, QPalette::Disabled),
           color(QPalette::Window, QPalette::Active, frameDarker),
           color(QPalette::Window, QPalette::Active, currentFrameDarker),
           color(QPalette::Base, QPalette::Active));

    if (!inWindows || darkTheme)
        styleSheet += "QToolBar { border:none }\n";

    setStyleSheet(styleSheet);

    // fix checkbox borders in dark theme
    if (darkTheme)
        palette.setColor(QPalette::Window, 0x666666);
    mUi->toolBarMain->setPalette(palette);
    mUi->menuView->setPalette(palette);
    mEditorManager.setEditorToolBarPalette(palette);

    style()->unpolish(qApp);
    style()->polish(qApp);
}

void MainWindow::handleHideMenuBarChanged(bool hide) 
{
    mUi->menubar->setVisible(!hide);
    mMenuButton->setVisible(hide);
    if (hide) {
        mUi->toolBarMain->insertWidget(mUi->actionNew, mMenuButton);
        mUi->toolBarMain->insertSeparator(mUi->actionNew);
    }
    else {
        mUi->toolBarMain->removeAction(mUi->toolBarMain->actions().front());
        mUi->toolBarMain->removeAction(mUi->toolBarMain->actions().front());
    }
}

void MainWindow::openSessionDock()
{
    mSessionDock->setVisible(true);
}

void MainWindow::openMessageDock()
{
    // only open once automatically
    if (QObject::sender() == mMessageWindow.data())
        disconnect(mMessageWindow.data(), &MessageWindow::messagesAdded,
            this, &MainWindow::openMessageDock);

    for (auto p = mMessageWindow->parentWidget(); p; p = p->parentWidget())
        p->setVisible(true);
}

void MainWindow::openContainingFolder()
{
    if (mEditorManager.hasCurrentEditor())
        return FileDialog::showInFileManager(mEditorManager.currentEditorFileName());

    return FileDialog::showInFileManager(mSessionEditor->fileName());
}

void MainWindow::populateSampleSessions()
{
    const auto paths = std::initializer_list<QString>{
        QCoreApplication::applicationDirPath() + "/samples",
#if !defined(NDEBUG)
        QCoreApplication::applicationDirPath() + "/../share/gpupad/samples",
        QCoreApplication::applicationDirPath() + "/../samples",
        QCoreApplication::applicationDirPath() + "/../../samples",
#endif
#if defined(__linux__)
        qEnvironmentVariable("APPDIR") + "/usr/share/gpupad/samples",
#endif
    };
    for (const auto &path : paths) {
        if (!mUi->menuSampleSessions->actions().empty())
            return;

        auto samples = QDir(path);
        if (samples.exists()) {
            samples.setFilter(QDir::Dirs | QDir::NoDotAndDotDot);
            const auto entries = samples.entryInfoList();
            for (const auto &entry : entries) {
                auto sample = QDir(entry.absoluteFilePath());
                sample.setNameFilters({ "*.gpjs" });
                auto sessions = sample.entryInfoList();
                if (!sessions.empty()) {
                    auto action = mUi->menuSampleSessions->addAction(
                        "&" + entry.fileName(), this, SLOT(openSampleSession()));
                    action->setData(sessions.first().absoluteFilePath());
                }
            }
        }
    }
}

void MainWindow::openSampleSession()
{
    const auto evalSteady = mUi->actionEvalSteady->isChecked();

    if (auto action = qobject_cast<QAction*>(QObject::sender()))
        if (openFile(action->data().toString())) {
            mUi->actionEvalSteady->setChecked(evalSteady);
            mUi->actionEvalAuto->setChecked(!evalSteady);
        }
}

void MainWindow::openOnlineHelp()
{
    QDesktopServices::openUrl(QUrl("https://github.com/houmain/gpupad"));
}

void MainWindow::openAbout()
{
    auto about = AboutDialog(this);
    about.setModal(true);
    about.exec();
}
