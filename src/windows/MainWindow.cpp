#include "MainWindow.h"
#include "AboutDialog.h"
#include "AutoOrientationSplitter.h"
#include "FileBrowserWindow.h"
#include "InputState.h"
#include "MessageList.h"
#include "MessageWindow.h"
#include "OutputWindow.h"
#include "Settings.h"
#include "Singletons.h"
#include "SynchronizeLogic.h"
#include "Theme.h"
#include "WindowTitle.h"
#include "editors/EditorManager.h"
#include "editors/IEditor.h"
#include "getEventPosition.h"
#include "scripting/CustomActions.h"
#include "session/SessionEditor.h"
#include "session/SessionModel.h"
#include "session/PropertiesEditor.h"
#include "ui_MainWindow.h"
#include <QActionGroup>
#include <QCloseEvent>
#include <QCoreApplication>
#include <QDesktopServices>
#include <QDockWidget>
#include <QMenu>
#include <QMessageBox>
#include <QMimeData>
#include <QOpenGLWidget>
#include <QScreen>
#include <QTimer>
#include <QToolButton>

void setFileDialogDirectory(const QString &fileName)
{
    if (!FileDialog::isEmptyOrUntitled(fileName))
        Singletons::fileDialog().setDirectory(QFileInfo(fileName).dir());
}

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , mUi(new Ui::MainWindow)
    , mMessageWindow(std::make_unique<MessageWindow>())
    , mSingletons(new Singletons(this))
    , mOutputWindow(std::make_unique<OutputWindow>())
    , mFileBrowserWindow(std::make_unique<FileBrowserWindow>())
    , mEditorManager(Singletons::editorManager())
    , mSessionEditor(std::make_unique<SessionEditor>())
    , mPropertiesEditor(std::make_unique<PropertiesEditor>())
{
    mUi->setupUi(this);
    setFont(qApp->font());

    setAcceptDrops(true);

    auto icon = QIcon(":images/16x16/icon.png");
    icon.addFile(":images/32x32/icon.png");
    icon.addFile(":images/64x64/icon.png");
    setWindowIcon(icon);

    mUi->menubar->setFixedHeight(24);
    mUi->toolBarMain->toggleViewAction()->setVisible(false);

    takeCentralWidget();

#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
    // WORKAROUND: trigger initialization of OpenGL immediately, otherwise
    // application window disappears momentaryly when the first texture editor is opened
    (new QOpenGLWidget(this))->setVisible(false);
#endif

    setContentsMargins(2, 0, 2, 2);
    auto content = new QWidget(this);
    mEditorManager.setParent(content);
    auto layout = new QVBoxLayout(content);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);
    layout->addWidget(&mEditorManager);
    layout->addWidget(
        mEditorManager.createEditorPropertiesPanel(mUi->actionFindReplace));

    mFullScreenBar = new QToolBar(this);
    mFullScreenTitle = new QLabel(this);
    auto fullScreenCloseButton = new QToolButton(this);
    fullScreenCloseButton->setDefaultAction(mUi->actionFullScreenClose);
    fullScreenCloseButton->setMaximumSize(16, 16);
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
    mMenuButton->setIcon(QIcon::fromTheme("hamburger-menu"));
    mMenuButton->setPopupMode(QToolButton::InstantPopup);

    auto dock = new QDockWidget(this);
    dock->setWidget(content);
    dock->setObjectName("Editors");
    dock->setFeatures(QDockWidget::NoDockWidgetFeatures);
    dock->setTitleBarWidget(new QWidget(this));
    dock->toggleViewAction()->setVisible(false);
    dock->setMinimumSize(150, 150);
    addDockWidget(Qt::RightDockWidgetArea, dock);
    auto editorsDock = dock;

    mSessionSplitter = new AutoOrientationSplitter(this);
    mSessionSplitter->addWidget(mSessionEditor.get());
    mSessionSplitter->addWidget(mPropertiesEditor.get());
    mSessionEditor->setMinimumSize(100, 100);
    mPropertiesEditor->setMinimumSize(100, 100);

    dock = new QDockWidget(tr("Session"), this);
    dock->setObjectName("Session");
    dock->setTitleBarWidget(new WindowTitle(dock));
    dock->setFeatures(QDockWidget::DockWidgetClosable
        | QDockWidget::DockWidgetMovable);
    dock->setWidget(mSessionSplitter);
    dock->setVisible(false);
    dock->setMinimumSize(200, 150);
    auto action = dock->toggleViewAction();
    action->setObjectName("toggle" + dock->objectName());
    action->setText(tr("Show &") + action->text());
    action->setIcon(QIcon::fromTheme("format-indent-more"));
    mUi->menuView->addAction(action);
    mUi->toolBarMain->insertAction(mUi->actionEvalReset, action);
    addDockWidget(Qt::LeftDockWidgetArea, dock);
    mSessionEditor->addItemActions(mUi->menuSession);
    mSessionDock = dock;

    dock = new QDockWidget(tr("File-Browser"), this);
    dock->setObjectName("FileBrowser");
    dock->setTitleBarWidget(mFileBrowserWindow->titleBar());
    dock->setFeatures(QDockWidget::DockWidgetClosable
        | QDockWidget::DockWidgetMovable);
    dock->setWidget(mFileBrowserWindow.get());
    dock->setVisible(false);
    dock->setMinimumSize(150, 150);
    action = dock->toggleViewAction();
    action->setObjectName("toggle" + dock->objectName());
    action->setText(tr("Show &") + action->text());
    action->setIcon(QIcon::fromTheme("folder"));
    mUi->menuView->addAction(action);
    mUi->toolBarMain->insertAction(mUi->actionEvalReset, action);
    splitDockWidget(mSessionDock, dock, Qt::Vertical);

    dock = new QDockWidget(tr("Messages"), this);
    dock->setObjectName("Messages");
    dock->setTitleBarWidget(new WindowTitle(dock));
    dock->setFeatures(QDockWidget::DockWidgetClosable
        | QDockWidget::DockWidgetMovable);
    dock->setWidget(mMessageWindow.get());
    dock->setVisible(false);
    dock->setMinimumSize(150, 150);
    action = dock->toggleViewAction();
    action->setObjectName("toggle" + dock->objectName());
    action->setText(tr("Show &") + action->text());
    action->setIcon(QIcon::fromTheme("help-faq"));
    mUi->menuView->addAction(action);
    mUi->toolBarMain->insertAction(mUi->actionEvalReset, action);
    addDockWidget(Qt::RightDockWidgetArea, dock);

    dock = new QDockWidget(tr("Output"), this);
    dock->setObjectName("Output");
    dock->setTitleBarWidget(mOutputWindow->titleBar());
    dock->setFeatures(QDockWidget::DockWidgetClosable
        | QDockWidget::DockWidgetMovable);
    dock->setWidget(mOutputWindow.get());
    dock->setVisible(false);
    dock->setMinimumSize(150, 150);
    action = dock->toggleViewAction();
    action->setObjectName("toggle" + dock->objectName());
    action->setText(tr("Show &") + action->text());
    action->setIcon(QIcon::fromTheme("utilities-terminal"));
    mUi->menuView->addAction(action);
    mUi->toolBarMain->insertAction(mUi->actionEvalReset, action);
    splitDockWidget(editorsDock, dock, Qt::Horizontal);
    auto outputDock = dock;

    mUi->toolBarMain->insertSeparator(mUi->actionEvalReset);

    mUi->actionQuit->setShortcuts(QKeySequence::Quit);
    mUi->actionNew->setShortcuts(QKeySequence::New);
    mUi->actionOpen->setShortcuts(QKeySequence::Open);
    mUi->actionSave->setShortcuts(QKeySequence::Save);
    mUi->actionSaveAs->setShortcuts(QKeySequence::SaveAs);
    mUi->actionSaveAll->setShortcut(QKeySequence("Ctrl+Shift+S"));
    mUi->actionClose->setShortcuts(QKeySequence::Close);
    mUi->actionUndo->setShortcuts(QKeySequence::Undo);
    mUi->actionRedo->setShortcuts(QKeySequence::Redo);
    mUi->actionCut->setShortcuts(QKeySequence::Cut);
    mUi->actionCopy->setShortcuts(QKeySequence::Copy);
    mUi->actionPaste->setShortcuts(QKeySequence::Paste);
    mUi->actionDelete->setShortcuts(QKeySequence::Delete);
    mUi->actionSelectAll->setShortcuts(QKeySequence::SelectAll);
    mUi->actionOnlineHelp->setShortcuts(QKeySequence::HelpContents);
    mUi->actionFindReplace->setShortcuts(
        { QKeySequence::Find, QKeySequence::Replace });

    addAction(mUi->actionFocusNextEditor);
    addAction(mUi->actionFocusPreviousEditor);

    auto windowFileName = new QAction(this);
    mEditActions = EditActions{ windowFileName, mUi->actionUndo,
        mUi->actionRedo, mUi->actionCut, mUi->actionCopy, mUi->actionPaste,
        mUi->actionDelete, mUi->actionSelectAll, mUi->actionRename,
        mUi->actionFindReplace };

    connect(mUi->actionNew, &QAction::triggered, this, &MainWindow::newFile);
    connect(mUi->actionOpen, &QAction::triggered, this,
        static_cast<void (MainWindow::*)()>(&MainWindow::openFile));
    connect(mUi->actionReload, &QAction::triggered, this,
        &MainWindow::reloadFile);
    connect(mUi->actionSave, &QAction::triggered, this, &MainWindow::saveFile);
    connect(mUi->actionSaveAs, &QAction::triggered, this,
        &MainWindow::saveFileAs);
    connect(mUi->actionSaveAll, &QAction::triggered, this,
        &MainWindow::saveAllFiles);
    connect(mUi->actionClose, &QAction::triggered, this,
        &MainWindow::closeFile);
    connect(mUi->actionCloseAll, &QAction::triggered, this,
        &MainWindow::closeAllFiles);
    connect(mUi->actionQuit, &QAction::triggered, this, &MainWindow::close);
    connect(mUi->actionFullScreenClose, &QAction::triggered, this,
        &MainWindow::close);
    connect(mUi->actionOpenContainingFolder, &QAction::triggered, this,
        &MainWindow::openContainingFolder);
    connect(mUi->actionOnlineHelp, &QAction::triggered, this,
        &MainWindow::openOnlineHelp);
    connect(mUi->menuWindowThemes, &QMenu::aboutToShow, this,
        &MainWindow::populateThemesMenu);
    connect(mUi->menuEditorThemes, &QMenu::aboutToShow, this,
        &MainWindow::populateThemesMenu);
    connect(mUi->menuSampleSessions, &QMenu::aboutToShow, this,
        &MainWindow::populateSampleSessions);
    connect(mUi->menuRecentFiles, &QMenu::aboutToShow, this,
        &MainWindow::updateRecentFileActions);
    connect(mUi->actionAbout, &QAction::triggered, this,
        &MainWindow::openAbout);
    connect(windowFileName, &QAction::changed, this,
        &MainWindow::updateFileActions);
    connect(mUi->actionFocusNextEditor, &QAction::triggered, this,
        &MainWindow::focusNextEditor);
    connect(mUi->actionFocusPreviousEditor, &QAction::triggered, this,
        &MainWindow::focusPreviousEditor);
    connect(mUi->actionNavigateBackward, &QAction::triggered, &mEditorManager,
        &EditorManager::navigateBackward);
    connect(mUi->actionNavigateForward, &QAction::triggered, &mEditorManager,
        &EditorManager::navigateForward);
    connect(&mEditorManager, &EditorManager::canNavigateBackwardChanged,
        mUi->actionNavigateBackward, &QAction::setEnabled);
    connect(&mEditorManager, &EditorManager::canNavigateForwardChanged,
        mUi->actionNavigateForward, &QAction::setEnabled);
    connect(&mEditorManager, &EditorManager::canPasteInNewEditorChanged,
        mUi->actionPasteInNewEditor, &QAction::setEnabled);
    connect(mUi->actionPasteInNewEditor, &QAction::triggered, &mEditorManager,
        &EditorManager::pasteInNewEditor);
    connect(qApp, &QApplication::focusChanged, this,
        &MainWindow::updateCurrentEditor);
    connect(mUi->actionFullScreen, &QAction::triggered, this,
        &MainWindow::setFullScreen);
    connect(mSessionEditor->selectionModel(),
        &QItemSelectionModel::currentChanged, mPropertiesEditor.get(),
        &PropertiesEditor::setCurrentModelIndex);
    connect(mSessionEditor.get(), &SessionEditor::itemAdded, this,
        &MainWindow::openSessionDock);
    connect(mSessionEditor.get(), &SessionEditor::itemActivated,
        mPropertiesEditor.get(), &PropertiesEditor::openItemEditor);
    connect(mUi->menuSession, &QMenu::aboutToShow, mSessionEditor.get(),
        &SessionEditor::updateItemActions);
    connect(&mEditorManager, &DockWindow::openNewDock, this,
        &MainWindow::newFile);
    connect(mFileBrowserWindow.get(), &FileBrowserWindow::fileActivated,
        [this](const QString &fileName) { openFile(fileName); });

    auto &synchronizeLogic = Singletons::synchronizeLogic();
    connect(mOutputWindow.get(), &OutputWindow::typeSelectionChanged,
        &synchronizeLogic, &SynchronizeLogic::setProcessSourceType);
    connect(outputDock, &QDockWidget::visibilityChanged, [this](bool visible) {
        Singletons::synchronizeLogic().setProcessSourceType(
            visible ? mOutputWindow->selectedType() : "");
    });
    connect(mMessageWindow.get(), &MessageWindow::messageActivated, this,
        &MainWindow::handleMessageActivated);
    connect(mMessageWindow.get(), &MessageWindow::messagesAdded, this,
        &MainWindow::openMessageDock);
    connect(&synchronizeLogic, &SynchronizeLogic::outputChanged,
        [this](const QVariant &value) {
            mOutputWindow->setText(value.toString());
        });
    connect(&Singletons::inputState(), &InputState::mouseChanged,
        &synchronizeLogic, &SynchronizeLogic::handleMouseStateChanged);
    connect(&Singletons::inputState(), &InputState::keysChanged,
        &synchronizeLogic, &SynchronizeLogic::handleKeyboardStateChanged);

    auto &settings = Singletons::settings();
    connect(mUi->actionSelectFont, &QAction::triggered, &settings,
        &Settings::selectFont);
    connect(mUi->actionShowWhiteSpace, &QAction::toggled, &settings,
        &Settings::setShowWhiteSpace);
    connect(mUi->actionHideMenuBar, &QAction::toggled, &settings,
        &Settings::setHideMenuBar);
    connect(mUi->actionLineWrapping, &QAction::toggled, &settings,
        &Settings::setLineWrap);
    connect(mUi->actionIndentWithSpaces, &QAction::toggled, &settings,
        &Settings::setIndentWithSpaces);
    connect(&settings, &Settings::windowThemeChanging, this,
        &MainWindow::handleThemeChanging);
    connect(&settings, &Settings::hideMenuBarChanged, this,
        &MainWindow::handleHideMenuBarChanged);

    connect(mUi->actionEvalReset, &QAction::triggered, this,
        &MainWindow::updateEvaluationMode);
    connect(mUi->actionEvalManual, &QAction::triggered, this,
        &MainWindow::updateEvaluationMode);
    connect(mUi->actionEvalAuto, &QAction::toggled, this,
        &MainWindow::updateEvaluationMode);
    connect(mUi->actionEvalSteady, &QAction::toggled, this,
        &MainWindow::updateEvaluationMode);

    connect(mUi->menuCustomActions, &QMenu::aboutToShow, this,
        &MainWindow::updateCustomActionsMenu);

    auto *customActionsButton = static_cast<QToolButton *>(
        mUi->toolBarMain->widgetForAction(mUi->actionCustomActions));
    customActionsButton->setMenu(mUi->menuCustomActions);
    customActionsButton->setPopupMode(QToolButton::InstantPopup);

    qApp->installEventFilter(this);

    mUi->actionPasteInNewEditor->setEnabled(
        mEditorManager.canPasteInNewEditor());

    auto indentActionGroup = new QActionGroup(this);
    connect(indentActionGroup, &QActionGroup::triggered, [](QAction *a) {
        Singletons::settings().setTabSize(a->text().toInt());
    });
    for (auto i = 1; i <= 8; i++) {
        auto action = mUi->menuTabSize->addAction(QString::number(i));
        action->setCheckable(true);
        action->setChecked(i == settings.tabSize());
        action->setActionGroup(indentActionGroup);
    }

    for (auto i = 0; i < 9; ++i) {
        auto action = mUi->menuRecentFiles->addAction("");
        connect(action, &QAction::triggered, this, &MainWindow::openRecentFile);
        mRecentSessionActions += action;
    }

    mUi->menuRecentFiles->addSeparator();

    for (auto i = 0; i < 26; ++i) {
        auto action = mUi->menuRecentFiles->addAction("");
        connect(action, &QAction::triggered, this, &MainWindow::openRecentFile);
        mRecentFileActions += action;
    }

    readSettings();
    settings.applyTheme();
}

MainWindow::~MainWindow()
{
    disconnect(qApp, &QApplication::focusChanged, this,
        &MainWindow::updateCurrentEditor);

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

    auto hiddenIcons = QStringList();
    for (auto action : mUi->toolBarMain->actions())
        if (!action->isVisible() && !action->objectName().isEmpty())
            hiddenIcons += action->objectName();
    settings.setValue("hiddenIcons", hiddenIcons);
}

void MainWindow::readSettings()
{
    auto &settings = Singletons::settings();
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
        mSessionSplitter->restoreState(
            settings.value("sessionSplitter").toByteArray());
    });

    Singletons::fileDialog().setDirectory(
        settings.value("lastDirectory").toString());

    mRecentFiles = settings.value("recentFiles").toStringList();
    mUi->actionIndentWithSpaces->setChecked(settings.indentWithSpaces());
    mUi->actionShowWhiteSpace->setChecked(settings.showWhiteSpace());
    mUi->actionHideMenuBar->setChecked(settings.hideMenuBar());
    mUi->actionLineWrapping->setChecked(settings.lineWrap());
    mUi->actionFullScreen->setChecked(isFullScreen());
    if (settings.hideMenuBar())
        handleHideMenuBarChanged(true);

    auto prevVisibleAction = std::add_pointer_t<QAction>{};
    const auto hiddenIconNames = settings.value("hiddenIcons").toStringList();
    for (auto action : mUi->toolBarMain->actions()) {
        if (action->isSeparator())
            action->setVisible(prevVisibleAction
                && !prevVisibleAction->isSeparator());

        if (hiddenIconNames.contains(action->objectName())) {
            setToolbarIconVisible(action, false);
        } else {
            prevVisibleAction = action;
        }
    }
}

void MainWindow::dragEnterEvent(QDragEnterEvent *event)
{
    if (event->mimeData()->hasUrls()) {
        event->acceptProposedAction();
    }
}

void MainWindow::dropEvent(QDropEvent *event)
{
    // drop on editor at position
    if (auto widget = qApp->widgetAt(mapToGlobal(getEventPosition(event)))) {
        widget->setFocus();
        updateCurrentEditor();
    }
    const auto urls = event->mimeData()->urls();
    for (const QUrl &url : std::as_const(urls))
        openFile(toNativeCanonicalFilePath(url.toLocalFile()));

    raise();
    activateWindow();
}

bool MainWindow::eventFilter(QObject *watched, QEvent *event)
{
    switch (event->type()) {
    case QEvent::KeyPress:
    case QEvent::MouseButtonPress:
    case QEvent::MouseButtonRelease:
    case QEvent::MouseMove:
    case QEvent::Wheel:              mLastPressWasAlt = false; break;
    default:                         break;
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
    if (mMenuButton->isVisible())
        if (event->key() == Qt::Key_Alt && mLastPressWasAlt)
            if (!std::exchange(mIgnoreNextAlt, false))
                return mMenuButton->click();

    mLastPressWasAlt = false;

    QMainWindow::keyReleaseEvent(event);
}

void MainWindow::setToolbarIconVisible(QAction *action, bool visible)
{
    if (action->isVisible() == visible)
        return;

    // do not simply set visible of action, since it would also
    // disappear in menu. Therefore replace with an invisble
    // placeholder, which allows to restore the original
    auto menu = mUi->toolBarMain;
    if (!visible) {
        auto placeholder = new QAction(menu);
        placeholder->setObjectName(action->objectName());
        placeholder->setData(QVariant::fromValue(action));
        placeholder->setText(action->text());
        placeholder->setVisible(false);
        menu->insertAction(action, placeholder);
        menu->removeAction(action);
    } else {
        auto placeholder = action;
        action = qvariant_cast<QAction *>(placeholder->data());
        menu->insertAction(placeholder, action);
        menu->removeAction(placeholder);
        placeholder->deleteLater();
    }
}

QMenu *MainWindow::createPopupMenu()
{
    const auto menu = QMainWindow::createPopupMenu();
    const auto toggleVisibleMenu = new QMenu("Show &Toolbar Icons", menu);
    for (auto action : mUi->toolBarMain->actions()) {
        if (action->isSeparator()) {
            toggleVisibleMenu->addSeparator();
            continue;
        }
        if (action->text().isEmpty())
            continue;
        auto toggleVisible = new QAction(action->text(), menu);
        toggleVisible->setCheckable(true);
        toggleVisible->setChecked(action->isVisible());
        connect(toggleVisible, &QAction::toggled, [this, action](bool checked) {
            setToolbarIconVisible(action, checked);
        });
        toggleVisibleMenu->addAction(toggleVisible);
    }

    const auto firstSeparator = [&]() {
        auto separator = std::add_pointer_t<QAction>{};
        const auto actions = menu->actions();
        for (auto it = actions.rbegin(); it != actions.rend(); ++it) {
            auto action = *it;
            if (!action->isVisible())
                continue;
            if (!action->isSeparator())
                break;
            separator = action;
        }
        return separator;
    }();
    menu->insertMenu(firstSeparator, toggleVisibleMenu);
    menu->addSeparator();
    menu->addAction(mUi->actionHideMenuBar);
    menu->addAction(mUi->actionFullScreen);
    return menu;
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
    if (!mEditorManager.focusNextEditor(!mSessionDock->isVisible()))
        mSessionEditor->setFocus();
}

void MainWindow::focusPreviousEditor()
{
    if (!mEditorManager.focusPreviousEditor(!mSessionDock->isVisible()))
        mSessionEditor->setFocus();
}

void MainWindow::setFullScreen(bool fullScreen)
{
    if (fullScreen) {
        setContentsMargins(0, 0, 0, 0);
        showFullScreen();
        mFullScreenBar->show();
        updateFileActions();
    } else {
        setContentsMargins(1, 1, 1, 1);
        mFullScreenBar->hide();
        if (mSingletons->settings().value("maximized").toBool()) {
            showMaximized();
        } else {
            showNormal();
        }
    }
}

void MainWindow::updateCurrentEditor()
{
    // do not switch, when there are both
    // session and editors, but none is focused
    auto focusWidget = qApp->focusWidget();
    if (mEditorManager.hasEditor() && !mEditorManager.isAncestorOf(focusWidget)
        && !mSessionSplitter->isAncestorOf(focusWidget))
        return;

    mEditorManager.updateCurrentEditor();
    disconnectEditActions();
    connectEditActions();
    updateDockCurrentProperty(mSessionDock, !mEditorManager.hasCurrentEditor());
    setFileDialogDirectory(mEditorManager.hasCurrentEditor()
            ? mEditorManager.currentEditorFileName()
            : mSessionEditor->fileName());
}

void MainWindow::disconnectEditActions()
{
    for (const auto &connection : std::as_const(mConnectedEditActions))
        disconnect(connection);
    mConnectedEditActions.clear();

    auto actions = { mEditActions.undo, mEditActions.redo, mEditActions.cut,
        mEditActions.copy, mEditActions.paste, mEditActions.delete_,
        mEditActions.selectAll, mEditActions.rename, mEditActions.findReplace };
    for (auto action : actions)
        action->setEnabled(false);
}

void MainWindow::connectEditActions()
{
    mConnectedEditActions = mEditorManager.hasCurrentEditor()
        ? mEditorManager.connectEditActions(mEditActions)
        : mSessionEditor->connectEditActions(mEditActions);
}

void MainWindow::updateFileActions()
{
    const auto fileName = mEditActions.windowFileName->text();
    const auto modified = mEditActions.windowFileName->isEnabled();
    auto title = QString((modified ? "*" : "")
        + FileDialog::getFullWindowTitle(fileName) + " - "
        + qApp->applicationName());
    setWindowTitle(title);

    if (isFullScreen()) {
        title.replace("[*]", "");
        mFullScreenTitle->setText(title + "  ");
        mFullScreenBar->hide();
        mFullScreenBar->show();
    }

    const auto desc =
        QString(" \"%1\"").arg(FileDialog::getFileTitle(fileName));
    mUi->actionSave->setText(tr("&Save%1").arg(desc));
    mUi->actionSaveAs->setText(tr("Save%1 &As...").arg(desc));
    mUi->actionClose->setText(tr("&Close%1").arg(desc));

    const auto hasFile = !FileDialog::isEmptyOrUntitled(fileName);
    mUi->actionReload->setEnabled(hasFile);
    mUi->actionReload->setText(tr("&Reload%1").arg(hasFile ? desc : ""));
    mUi->actionOpenContainingFolder->setEnabled(hasFile);
}

void MainWindow::stopSteadyEvaluation()
{
    if (mUi->actionEvalSteady->isChecked())
        setEvaluationMode(EvaluationMode::Automatic);
}

void MainWindow::setEvaluationMode(EvaluationMode evaluationMode)
{
    if (evaluationMode == EvaluationMode::Automatic) {
        mUi->actionEvalAuto->setChecked(true);
    } else if (evaluationMode == EvaluationMode::Steady) {
        mUi->actionEvalSteady->setChecked(true);
    } else {
        mUi->actionEvalSteady->setChecked(false);
        mUi->actionEvalAuto->setChecked(false);
    }
}

void MainWindow::updateEvaluationMode()
{
    if (QObject::sender() == mUi->actionEvalReset) {
        Singletons::synchronizeLogic().resetEvaluation();
    } else if (QObject::sender() == mUi->actionEvalManual) {
        Singletons::synchronizeLogic().manualEvaluation();
    } else if (QObject::sender() == mUi->actionEvalAuto) {
        if (mUi->actionEvalAuto->isChecked())
            mUi->actionEvalSteady->setChecked(false);
    } else {
        if (mUi->actionEvalSteady->isChecked())
            mUi->actionEvalAuto->setChecked(false);
    }

    Singletons::synchronizeLogic().setEvaluationMode(
        mUi->actionEvalAuto->isChecked()         ? EvaluationMode::Automatic
            : mUi->actionEvalSteady->isChecked() ? EvaluationMode::Steady
                                                 : EvaluationMode::Paused);
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
    auto options = FileDialog::Options{ FileDialog::Multiselect
        | FileDialog::AllExtensionFilters };
    auto &dialog = Singletons::fileDialog();
    if (dialog.exec(options)) {
        const auto fileNames = dialog.fileNames();
        for (const QString &fileName : fileNames)
            openFile(fileName, dialog.asBinaryFile());
    }
}

bool MainWindow::openFile(const QString &fileName_, bool asBinaryFile)
{
    const auto fileName = toNativeCanonicalFilePath(fileName_);
    if (FileDialog::isSessionFileName(fileName)) {
        if (!openSession(fileName))
            return false;
    } else {
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
    } else {
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
    } else {
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

    Singletons::synchronizeLogic().handleSessionFileNameChanged(fileName);

    if (!restoreSessionState(fileName)) {
        openSessionDock();
        auto &editors = Singletons::editorManager();
        editors.setAutoRaise(false);
        mSessionEditor->activateFirstTextureItem();
        editors.setAutoRaise(true);
    }
    return true;
}

bool MainWindow::saveSession()
{
    if (FileDialog::isUntitled(mSessionEditor->fileName())
        || !mSessionEditor->save())
        return saveSessionAs();

    saveSessionState(mSessionEditor->fileName());
    addToRecentFileList(mSessionEditor->fileName());
    return true;
}

bool MainWindow::saveSessionAs()
{
    auto options = FileDialog::Options{ FileDialog::Saving
        | FileDialog::SessionExtensions };
    const auto prevFileName = mSessionEditor->fileName();
    for (;;) {
        if (!Singletons::fileDialog().exec(options,
                mSessionEditor->fileName())) {
            mSessionEditor->setFileName(prevFileName);
            return false;
        }
        mSessionEditor->setFileName(Singletons::fileDialog().fileName());
        if (mSessionEditor->save())
            break;
    }

    if (!copySessionFiles(QFileInfo(prevFileName).path(),
            QFileInfo(mSessionEditor->fileName()).path()))
        showCopyingSessionFailedMessage(this);

    mSessionEditor->save();
    mSessionEditor->clearUndo();
    saveSessionState(mSessionEditor->fileName());
    addToRecentFileList(mSessionEditor->fileName());
    return true;
}

bool MainWindow::copySessionFiles(const QString &fromPath,
    const QString &toPath)
{
    const auto startsWithPath = [](const QString &string,
                                    const QString &start) {
        if (start.length() > string.length())
            return false;
        const auto isSlash = [](QChar c) { return (c == '/' || c == '\\'); };
        for (auto i = 0; i < start.length(); ++i)
            if (start[i] != string[i]
                && (!isSlash(string[i]) || !isSlash(start[i])))
                return false;
        return true;
    };

    auto succeeded = true;
    auto &model = Singletons::sessionModel();
    model.forEachFileItem([&](const FileItem &fileItem) {
        const auto prevFileName = fileItem.fileName;
        if (startsWithPath(prevFileName, fromPath)) {
            const auto newFileName = toNativeCanonicalFilePath(
                toPath + prevFileName.mid(fromPath.length()));

            model.setData(model.getIndex(&fileItem, SessionModel::FileName),
                newFileName);

            // rename editor filenames
            Singletons::editorManager().renameEditors(prevFileName,
                newFileName);

            if (!QFileInfo::exists(newFileName)) {
                QDir().mkpath(QFileInfo(newFileName).path());
                succeeded &= QFile(prevFileName).copy(newFileName);
            }
        }
    });
    return succeeded;
}

void MainWindow::saveSessionState(const QString &sessionFileName)
{
    const auto sessionStateFile = QString(sessionFileName + ".state");
    const auto sessionDir = QFileInfo(sessionFileName).dir();
    auto settings = QSettings(sessionStateFile, QSettings::IniFormat);

    // add file items by id (may not have a filename)
    auto openEditors = QStringList();
    auto filesAdded = QStringList();
    mSingletons->sessionModel().forEachFileItem([&](const FileItem &item) {
        if (auto editor = mEditorManager.getEditor(item.fileName)) {
            openEditors += QString("%1|%2").arg(item.id).arg(
                mEditorManager.getEditorObjectName(editor));
            filesAdded += item.fileName;
        }
    });

    // add other editors by relative filename
    mEditorManager.forEachEditor([&](const IEditor &editor) {
        // not saving/restoring Qml views for now
        if (FileDialog::getFileExtension(editor.fileName()) == "qml")
            return;

        if (!filesAdded.contains(editor.fileName()))
            openEditors += QString("%1|%2").arg(
                QDir::fromNativeSeparators(
                    sessionDir.relativeFilePath(editor.fileName())),
                mEditorManager.getEditorObjectName(&editor));
    });

    settings.setValue("editorState", mEditorManager.saveState());
    settings.setValue("openEditors", openEditors);
    auto &synchronizeLogic = Singletons::synchronizeLogic();
    settings.setValue("evaluationMode",
        static_cast<int>(synchronizeLogic.evaluationMode()));
}

bool MainWindow::restoreSessionState(const QString &sessionFileName)
{
    const auto sessionStateFile = QString(sessionFileName + ".state");
    const auto sessionDir = QFileInfo(sessionFileName).dir();
    if (!QFileInfo::exists(sessionStateFile))
        return false;

    auto &model = Singletons::sessionModel();
    auto settings = QSettings(sessionStateFile, QSettings::IniFormat);
    const auto openEditors = settings.value("openEditors").toStringList();
    mEditorManager.setAutoRaise(false);
    for (const auto &openEditor : openEditors)
        if (const auto sep = openEditor.indexOf('|'); sep > 0) {
            const auto identifier = openEditor.mid(0, sep);
            const auto editorObjectName = openEditor.mid(sep + 1);

            auto isItemId = false;
            if (const auto itemId = identifier.toInt(&isItemId); isItemId) {
                if (auto item = model.findItem(itemId))
                    if (auto editor = mPropertiesEditor->openItemEditor(
                            model.getIndex(item)))
                        mEditorManager.setEditorObjectName(editor,
                            editorObjectName);
            } else {
                const auto fileName = toNativeCanonicalFilePath(
                    sessionDir.absoluteFilePath(identifier));
                if (auto editor = mEditorManager.openEditor(fileName))
                    mEditorManager.setEditorObjectName(editor,
                        editorObjectName);
            }
        }
    mEditorManager.restoreState(settings.value("editorState").toByteArray());
    mEditorManager.setAutoRaise(true);

    setEvaluationMode(
        static_cast<EvaluationMode>(settings.value("evaluationMode").toInt()));
    return true;
}

bool MainWindow::closeSession()
{
    stopSteadyEvaluation();

    if (!mEditorManager.promptSaveAllEditors())
        return false;

    if (mSessionEditor->isModified()) {
        const auto ret = showNotSavedDialog(this, mSessionEditor->fileName());
        if (ret == QMessageBox::Cancel)
            return false;
        if (ret == QMessageBox::Save && !saveSession())
            return false;
    }

    Singletons::synchronizeLogic().handleSessionFileNameChanged("");
    mEditorManager.closeAllEditors(false);
    mOutputWindow->setText("");
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
        } else {
            if (recentFileIndex < mRecentFileActions.size())
                action = mRecentFileActions[recentFileIndex++];
        }
        if (action) {
            action->setText(filename);
            action->setData(filename);
            if (!filename.startsWith("\\"))
                action->setEnabled(QFile::exists(filename));
            action->setVisible(true);
        } else {
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
                auto text = action->text();
                if (text.size() > 100)
                    text = text.replace(45, text.size() - 90, "...  ...");
                action->setText(
                    QStringLiteral("  &%1 %2")
                        .arg(QChar(index < 9 ? '1' + index : 'A' + (index - 9)))
                        .arg(text));
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
    auto &customActions = Singletons::customActions();
    customActions.setSelection(
        mSessionEditor->selectionModel()->selectedIndexes());

    // keep actions referenced
    const auto actionPtrs = customActions.getApplicableActions();
    mUi->menuCustomActions->setProperty("actions",
        QVariant::fromValue(actionPtrs));

    auto actions = QList<QAction *>();
    for (const auto &actionPtr : actionPtrs)
        actions += actionPtr.get();

    if (actions.isEmpty()) {
        const static auto sEmpty = [&]() {
            auto action = new QAction(this);
            action->setText(tr("No custom actions"));
            action->setEnabled(false);
            return action;
        }();
        actions.append(sEmpty);
    }
    mUi->menuCustomActions->clear();
    mUi->menuCustomActions->addActions(actions);
}

void MainWindow::handleMessageActivated(ItemId itemId, QString fileName,
    int line, int column)
{
    if (itemId) {
        mSessionEditor->setCurrentItem(itemId);
        openSessionDock();
    } else if (!fileName.isEmpty()) {
        Singletons::editorManager().openSourceEditor(fileName, line, column);
    }
}

void MainWindow::handleThemeChanging(const Theme &theme)
{
    auto palette = theme.palette();
    qApp->setPalette(palette);

    const auto frameDarker = (theme.isDarkTheme() ? 70 : 120);
    const auto currentFrameDarker = (theme.isDarkTheme() ? 50 : 180);
    const auto color = [&](QPalette::ColorRole role, QPalette::ColorGroup group,
                           int darker = 100) {
        return palette.brush(group, role)
            .color()
            .darker(darker)
            .name(QColor::HexRgb);
    };
    const auto format = QStringLiteral(
        "QLabel:disabled { color: %2 }\n"
        "QDockWidget { color: %1; background-color: %5 }\n"
        "QDockWidget > QFrame { border:1px solid %3 }\n"
        "QDockWidget[current=true] > QFrame { border:1px solid %4 }\n"
        "QDockWidget > QFrame[no-bottom-border=true] { border-bottom: none }\n"
        "QMenuBar { color: %1; background-color: %5; border:none; margin:0; "
        "padding:0; "
        "padding-top:2px; }\n"
        "QToolButton { margin:2; margin:0px; padding:2px; }\n"
        "QToolBar { spacing:2px; margin:0; padding:2px; padding-left:4px; "
        "background-color: %5; border:none }\n");
    const auto styleSheet =
        format.arg(color(QPalette::WindowText, QPalette::Active),
            color(QPalette::WindowText, QPalette::Disabled),
            color(QPalette::Window, QPalette::Active, frameDarker),
            color(QPalette::Window, QPalette::Active, currentFrameDarker),
            color(QPalette::Base, QPalette::Active));

    setStyleSheet(styleSheet);

    // fix checkbox borders in dark theme
    if (theme.isDarkTheme())
        palette.setColor(QPalette::Window, 0x666666);
    mUi->toolBarMain->setPalette(palette);
    mUi->menuView->setPalette(palette);
    mEditorManager.setEditorToolBarPalette(palette);

    Singletons::sessionModel().setActiveItemColor(
        theme.getColor(ThemeColor::BuiltinConstant));

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
    } else {
        const auto &actions = mUi->toolBarMain->actions();
        mUi->toolBarMain->removeAction(actions.front());
    }
}

void MainWindow::openSessionDock()
{
    mSessionDock->setVisible(true);
}

void MainWindow::openMessageDock()
{
    // only open once automatically
    if (QObject::sender() == mMessageWindow.get())
        disconnect(mMessageWindow.get(), &MessageWindow::messagesAdded, this,
            &MainWindow::openMessageDock);

    for (auto p = mMessageWindow->parentWidget(); p; p = p->parentWidget())
        p->setVisible(true);
}

void MainWindow::openContainingFolder()
{
    if (mEditorManager.hasCurrentEditor())
        return showInFileManager(mEditorManager.currentEditorFileName());

    return showInFileManager(mSessionEditor->fileName());
}

void MainWindow::populateThemesMenu()
{
    if (!mUi->menuWindowThemes->actions().empty())
        return;

    const auto *currentWindowTheme = &Singletons::settings().windowTheme();
    const auto *currentEditorTheme = &Singletons::settings().editorTheme();
    auto windowThemeActionGroup = new QActionGroup(this);
    auto editorThemeActionGroup = new QActionGroup(this);

    const auto fileNames = Theme::getThemeFileNames();
    for (const auto &fileName : fileNames) {
        const auto &theme = Theme::getTheme(fileName);
        for (auto menu : { mUi->menuWindowThemes, mUi->menuEditorThemes }) {
            auto action =
                menu->addAction(theme.name(), this, SLOT(setSelectedTheme()));
            action->setData(QVariant::fromValue(&theme));
            action->setCheckable(true);
            action->setChecked(&theme
                == (menu == mUi->menuWindowThemes ? currentWindowTheme
                                                  : currentEditorTheme));
            action->setActionGroup(menu == mUi->menuWindowThemes
                    ? windowThemeActionGroup
                    : editorThemeActionGroup);
            if (fileName.isEmpty()) {
                action->setText("Default");
                menu->addSeparator();
            }
        }
    }
}

void MainWindow::setSelectedTheme()
{
    const auto action = qobject_cast<QAction *>(QObject::sender());
    const auto menu = qobject_cast<QMenu *>(action->parent());
    const auto &theme = *action->data().value<const Theme *>();
    if (menu == mUi->menuWindowThemes)
        Singletons::settings().setWindowTheme(theme);
    else
        Singletons::settings().setEditorTheme(theme);
}

void MainWindow::populateSampleSessions()
{
    if (!mUi->menuSampleSessions->actions().empty())
        return;
    const auto entries = enumerateApplicationPaths(SamplesDir, QDir::Dirs);
    for (const auto &entry : entries) {
        auto dir = QDir(entry.absoluteFilePath());
        dir.setNameFilters({ "*.gpjs" });
        auto sessions = dir.entryInfoList();
        if (!sessions.empty()) {
            auto action = mUi->menuSampleSessions->addAction(
                "&" + entry.fileName(), this, SLOT(openSampleSession()));
            action->setData(sessions.first().absoluteFilePath());
        }
    }
}

void MainWindow::openSampleSession()
{
    const auto evalSteady = mUi->actionEvalSteady->isChecked();

    if (auto action = qobject_cast<QAction *>(QObject::sender()))
        if (openFile(action->data().toString()))
            setEvaluationMode(evalSteady ? EvaluationMode::Steady
                                         : EvaluationMode::Automatic);
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
