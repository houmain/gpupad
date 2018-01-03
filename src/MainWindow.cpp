#include "MainWindow.h"
#include "ui_MainWindow.h"
#include "AutoOrientationSplitter.h"
#include "session/SessionEditor.h"
#include "session/SessionProperties.h"
#include "session/SessionModel.h"
#include "Singletons.h"
#include "MessageWindow.h"
#include "MessageList.h"
#include "Settings.h"
#include "SynchronizeLogic.h"
#include "editors/EditorManager.h"
#include "editors/FindReplaceBar.h"
#include "scripting/CustomActions.h"
#include <QCloseEvent>
#include <QMessageBox>
#include <QDockWidget>
#include <QDesktopServices>
#include <QActionGroup>
#include <QMenu>
#include <QToolButton>
#include <QCoreApplication>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , mUi(new Ui::MainWindow)
    , mMessageWindow(new MessageWindow())
    , mCustomActions(new CustomActions(this))
    , mSingletons(new Singletons(this))
    , mEditorManager(Singletons::editorManager())
    , mSessionEditor(new SessionEditor())
    , mSessionProperties(new SessionProperties())
{
    mUi->setupUi(this);

    auto icon = QIcon(":images/16x16/icon.png");
    icon.addFile(":images/32x32/icon.png");
    icon.addFile(":images/64x64/icon.png");
    setWindowIcon(icon);
    setContentsMargins(2, 0, 2, 2);

    takeCentralWidget();

    auto content = new QWidget(this);
    mEditorManager.setParent(content);
    auto layout = new QVBoxLayout(content);
    layout->setMargin(0);
    layout->setSpacing(0);
    layout->addWidget(&mEditorManager);
    layout->addWidget(&mEditorManager.findReplaceBar());

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
    mUi->menuView->addAction(dock->toggleViewAction());
    addDockWidget(Qt::LeftDockWidgetArea, dock);
    mSessionEditor->addItemActions(mUi->menuSession);

    dock = new QDockWidget(tr("Messages"), this);
    dock->setObjectName("Messages");
    dock->setFeatures(QDockWidget::DockWidgetClosable |
                      QDockWidget::DockWidgetMovable);
    dock->setWidget(mMessageWindow.data());
    dock->setVisible(false);
    mUi->menuView->addAction(dock->toggleViewAction());
    addDockWidget(Qt::RightDockWidgetArea, dock);

    mUi->menuView->addAction(mUi->toolBarMain->toggleViewAction());

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
    mUi->actionRename->setShortcut(QKeySequence("F2"));
    mUi->actionFindReplace->setShortcuts(QKeySequence::Find);

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
        this, &MainWindow::closeSession);
    connect(mUi->actionQuit, &QAction::triggered,
        this, &MainWindow::close);
    connect(mUi->actionOnlineHelp, &QAction::triggered,
        this, &MainWindow::openOnlineHelp);
    connect(mUi->menuHelp, &QMenu::aboutToShow,
        this, &MainWindow::populateSampleSessions);
    connect(mUi->actionAbout, &QAction::triggered,
        this, &MainWindow::openAbout);
    connect(windowFileName, &QAction::changed,
        this, &MainWindow::updateFileActions);
    connect(qApp, &QApplication::focusChanged,
        this, &MainWindow::updateCurrentEditor);
    connect(mSessionEditor->selectionModel(), &QItemSelectionModel::currentChanged,
        mSessionProperties.data(), &SessionProperties::setCurrentModelIndex);
    connect(mSessionEditor.data(), &SessionEditor::itemAdded,
        this, &MainWindow::openSessionDock);
    connect(mSessionEditor.data(), &SessionEditor::itemActivated,
        [this](const QModelIndex &index, bool* handled) {
            *handled = (mSessionProperties->openItemEditor(index) != nullptr);
        });
    connect(mUi->menuSession, &QMenu::aboutToShow,
        mSessionEditor.data(), &SessionEditor::updateItemActions);

    auto &synchronizeLogic = Singletons::synchronizeLogic();
    connect(&mEditorManager, &EditorManager::editorRenamed,
        &synchronizeLogic, &SynchronizeLogic::handleFileRenamed);
    connect(&mEditorManager, &EditorManager::editorChanged,
        &synchronizeLogic, &SynchronizeLogic::handleFileItemsChanged);
    connect(&mEditorManager, &EditorManager::sourceTypeChanged,
        &synchronizeLogic, &SynchronizeLogic::handleSourceTypeChanged);
    connect(mUi->actionSourceValidation, &QAction::toggled,
        &synchronizeLogic, &SynchronizeLogic::setSourceValidationActive);
    connect(mMessageWindow.data(), &MessageWindow::messageActivated,
        this, &MainWindow::handleMessageActivated);
    connect(mMessageWindow.data(), &MessageWindow::messagesAdded,
        this, &MainWindow::openMessageDock);

    auto &settings = Singletons::settings();
    connect(mUi->actionSelectFont, &QAction::triggered,
        &settings, &Settings::selectFont);
    connect(mUi->actionAutoIndentation, &QAction::toggled,
        &settings, &Settings::setAutoIndentation);
    connect(mUi->actionSyntaxHighlighting, &QAction::toggled,
        &settings, &Settings::setSyntaxHighlighting);
    connect(mUi->actionDarkTheme, &QAction::toggled,
        &settings, &Settings::setDarkTheme);
    connect(mUi->actionLineWrapping, &QAction::toggled,
        &settings, &Settings::setLineWrap);
    connect(mUi->actionIndentWithSpaces, &QAction::toggled,
        &settings, &Settings::setIndentWithSpaces);
    connect(&settings, &Settings::darkThemeChanged,
        this, &MainWindow::handleDarkThemeChanged);

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

    auto sourceTypeActionGroup = new QActionGroup(this);
    auto sourceTypeButton = qobject_cast<QToolButton*>(
        mUi->toolBarMain->widgetForAction(mUi->actionSourceValidation));
    sourceTypeButton->setMenu(mUi->menuSourceType);
    sourceTypeButton->setPopupMode(QToolButton::MenuButtonPopup);

    auto addSourceType = [&](const QString &text, SourceType sourceType) {
        auto action = mUi->menuSourceType->addAction(text);
        action->setData(static_cast<int>(sourceType));
        action->setCheckable(true);
        action->setActionGroup(sourceTypeActionGroup);
    };
    addSourceType(tr("Plaintext"), SourceType::PlainText);
    addSourceType(tr("Vertex Shader"), SourceType::VertexShader);
    addSourceType(tr("Fragment Shader"), SourceType::FragmentShader);
    addSourceType(tr("Geometry Shader"), SourceType::GeometryShader);
    addSourceType(tr("Tesselation Control"), SourceType::TesselationControl);
    addSourceType(tr("Tesselation Evaluation"), SourceType::TesselationEvaluation);
    addSourceType(tr("Compute Shader"), SourceType::ComputeShader);
    addSourceType(tr("JavaScript"), SourceType::JavaScript);

    connect(mUi->menuSourceType, &QMenu::aboutToShow,
        [this, sourceTypeActionGroup]() {
            auto sourceType = mEditorManager.currentSourceType();
            foreach (QAction *action, sourceTypeActionGroup->actions())
                action->setChecked(static_cast<SourceType>(
                    action->data().toInt()) == sourceType);
        });
    connect(mUi->menuSourceType, &QMenu::triggered,
        [this](QAction *action) {
            mEditorManager.setCurrentSourceType(
                static_cast<SourceType>(action->data().toInt()));
        });

    auto evalIntervalActionGroup = new QActionGroup(this);
    mUi->actionEvalIntervalSlow->setActionGroup(evalIntervalActionGroup);
    mUi->actionEvalIntervalMedium->setActionGroup(evalIntervalActionGroup);
    mUi->actionEvalIntervalFast->setActionGroup(evalIntervalActionGroup);
    mUi->actionEvalIntervalUnbounded->setActionGroup(evalIntervalActionGroup);
    connect(evalIntervalActionGroup, &QActionGroup::triggered,
        this, &MainWindow::updateEvaluationInterval);

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
        mRecentFileActions += action;
    }

    readSettings();
    updateEvaluationInterval();
}

MainWindow::~MainWindow()
{
    writeSettings();

    disconnect(qApp, &QApplication::focusChanged,
        this, &MainWindow::updateCurrentEditor);

    mSingletons.reset();
    delete mUi;
}

void MainWindow::writeSettings()
{
    auto &settings = Singletons::settings();
    if (!isMaximized())
        settings.setValue("geometry", saveGeometry());
    settings.setValue("maximized", isMaximized());
    settings.setValue("state", saveState());
    settings.setValue("sessionSplitter", mSessionSplitter->saveState());

    auto &fileDialog = Singletons::fileDialog();
    settings.setValue("lastDirectory", fileDialog.directory().absolutePath());

    settings.setValue("recentFiles", mRecentFiles);
}

void MainWindow::readSettings()
{
    auto &settings = Singletons::settings();
    resize(800, 600);

    auto geometry = settings.value("geometry").toByteArray();
    if (!geometry.isEmpty())
        restoreGeometry(geometry);
    else
        move(100, 100);

    if (settings.value("maximized").toBool())
        setWindowState(Qt::WindowMaximized);
    restoreState(settings.value("state").toByteArray());
    mSessionSplitter->restoreState(settings.value("sessionSplitter").toByteArray());

    auto &fileDialog = Singletons::fileDialog();
    fileDialog.setDirectory(settings.value("lastDirectory").toString());

    mRecentFiles = settings.value("recentFiles").toStringList();
    updateRecentFileActions();

    mUi->actionIndentWithSpaces->setChecked(settings.indentWithSpaces());
    mUi->actionAutoIndentation->setChecked(settings.autoIndentation());
    mUi->actionSyntaxHighlighting->setChecked(settings.syntaxHighlighting());
    mUi->actionDarkTheme->setChecked(settings.darkTheme());
    mUi->actionLineWrapping->setChecked(settings.lineWrap());
}

void MainWindow::closeEvent(QCloseEvent *event)
{
    if (closeSession())
        event->accept();
    else
        event->ignore();
}

void MainWindow::updateCurrentEditor()
{
    mEditorManager.updateCurrentEditor();
    disconnectEditActions();
    connectEditActions();
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
    if (mEditorManager.hasCurrentEditor()) {
        mConnectedEditActions = mEditorManager.connectEditActions(mEditActions);
    }
    else {
        auto focused = (qApp->focusWidget() == mSessionEditor.data());
        mConnectedEditActions = mSessionEditor->connectEditActions(
            mEditActions, focused);
    }
}

void MainWindow::updateFileActions()
{
    if (!isActiveWindow())
        return;

    auto fileName = mEditActions.windowFileName->text();
    auto modified = mEditActions.windowFileName->isEnabled();
    setWindowTitle((modified ? "*" : "") +
        FileDialog::getWindowTitle(fileName) +
        " - " + qApp->applicationName());

    const auto desc = QString(" \"%1\"").arg(FileDialog::getFileTitle(fileName));
    mUi->actionSave->setText(tr("&Save%1").arg(desc));
    mUi->actionSaveAs->setText(tr("Save%1 &As...").arg(desc));
    mUi->actionClose->setText(tr("&Close%1").arg(desc));

    const auto canReload = (mEditorManager.hasCurrentEditor());
    mUi->actionReload->setEnabled(canReload);
    mUi->actionReload->setText(tr("&Reload%1").arg(canReload ? desc : ""));

    auto sourceType = mEditorManager.currentSourceType();
    mUi->menuSourceType->setEnabled(sourceType != SourceType::None);
}

void MainWindow::stopEvaluation()
{
    mUi->actionEvalAuto->setChecked(false);
    mUi->actionEvalSteady->setChecked(false);
    Singletons::synchronizeLogic().resetRenderSession();
}

void MainWindow::updateEvaluationMode()
{
    if (QObject::sender() == mUi->actionEvalManual) {
        Singletons::synchronizeLogic().manualEvaluation();
    }
    else {
        if (QObject::sender() == mUi->actionEvalAuto) {
            if (mUi->actionEvalAuto->isChecked())
                mUi->actionEvalSteady->setChecked(false);
        }
        else if (mUi->actionEvalSteady->isChecked())
                mUi->actionEvalAuto->setChecked(false);
    }

    Singletons::synchronizeLogic().setEvaluationMode(
        mUi->actionEvalAuto->isChecked(),
        mUi->actionEvalSteady->isChecked());
}

void MainWindow::updateEvaluationInterval()
{
    Singletons::synchronizeLogic().setEvaluationInterval(
        mUi->actionEvalIntervalUnbounded->isChecked() ? 0 :
        mUi->actionEvalIntervalFast->isChecked() ? 15 :
        mUi->actionEvalIntervalMedium->isChecked() ? 100 :
        500);
}

bool MainWindow::hasEditor() const
{
    return mEditorManager.hasEditor();
}

void MainWindow::newFile()
{
    mEditorManager.openNewSourceEditor(tr("Untitled"),
        SourceType::FragmentShader);
}

void MainWindow::openFile()
{
    auto options = FileDialog::Options{
        FileDialog::Loading |
        FileDialog::AllExtensionFilters
    };
    if (Singletons::fileDialog().exec(options))
        foreach (QString fileName, Singletons::fileDialog().fileNames())
            openFile(fileName);
}

void MainWindow::openFile(const QString &fileName)
{
    if (FileDialog::isSessionFileName(fileName) ?
            openSession(fileName) :
            mEditorManager.openEditor(fileName))
        addToRecentFileList(fileName);
}

bool MainWindow::saveFile()
{
    if (mEditorManager.hasCurrentEditor())
        return mEditorManager.saveEditor();

    return saveSession();
}

bool MainWindow::saveFileAs()
{
    if (mEditorManager.hasCurrentEditor())
        return mEditorManager.saveEditorAs();

    return saveSessionAs();
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
    return mEditorManager.reloadEditor();
}

bool MainWindow::closeFile()
{
    if (mEditorManager.hasCurrentEditor())
        return mEditorManager.closeEditor();

    return closeSession();
}

bool MainWindow::openSession(const QString &fileName)
{
    if (!closeSession())
        return false;

    mSessionEditor->setFileName(fileName);
    openSessionDock();
    return mSessionEditor->load();
}

bool MainWindow::saveSession()
{
    if (FileDialog::isUntitled(mSessionEditor->fileName()) ||
        !QFileInfo(mSessionEditor->fileName()).isWritable())
        return saveSessionAs();

    return mSessionEditor->save();
}

bool MainWindow::saveSessionAs()
{
    auto options = FileDialog::Options{
        FileDialog::Saving |
        FileDialog::SessionExtensions
    };
    if (Singletons::fileDialog().exec(options, mSessionEditor->fileName())) {
        mSessionEditor->setFileName(Singletons::fileDialog().fileName());
        return saveSession();
    }
    return false;
}

bool MainWindow::closeSession()
{
    stopEvaluation();

    if (!mEditorManager.closeAllEditors())
        return false;

    if (mSessionEditor->isModified()) {
        auto ret = Singletons::editorManager().openNotSavedDialog(
            mSessionEditor->fileName());
        if (ret == QMessageBox::Cancel)
            return false;

        if (ret == QMessageBox::Save &&
            !saveSession())
            return false;
    }
    return mSessionEditor->clear();
}

void MainWindow::addToRecentFileList(const QString &fileName)
{
    mRecentFiles.removeAll(fileName);
    mRecentFiles.prepend(fileName);
    updateRecentFileActions();

    auto &fileDialog = Singletons::fileDialog();
    fileDialog.setDirectory(QFileInfo(fileName).dir());
}

void MainWindow::updateRecentFileActions()
{
    QMutableStringListIterator i(mRecentFiles);
    while (i.hasNext())
        if (!QFile::exists(i.next()))
            i.remove();

    while (mRecentFiles.size() > mRecentFileActions.size())
        mRecentFiles.pop_back();

    for (auto j = 0; j < mRecentFileActions.size(); ++j) {
        if (j < mRecentFiles.count()) {
            auto text = tr("&%1 %2").arg(j + 1).arg(mRecentFiles[j]);
            mRecentFileActions[j]->setText(text);
            mRecentFileActions[j]->setData(mRecentFiles[j]);
            mRecentFileActions[j]->setVisible(true);
        }
        else {
            mRecentFileActions[j]->setVisible(false);
        }
    }
    mUi->menuRecentFiles->setEnabled(!mRecentFiles.empty());
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
            fileName, true, line, column);
    }
}

void MainWindow::handleDarkThemeChanged(bool enabled)
{
    auto palette = qApp->style()->standardPalette();
    if (enabled) {
        struct S { QPalette::ColorRole role; QColor a; QColor i; QColor d; };
        for (auto s : std::initializer_list<S>{
                { QPalette::WindowText, 0x9F9F9F, 0x9F9F9F, 0x818181 },
                { QPalette::Button, 0x3B3B41, 0x3B3B41, 0x3B3B41 },
                { QPalette::Light, 0x6F6F6F, 0x6F6F6F, 0x4D4D53 },
                { QPalette::Text, 0xCFCFCF, 0xCFCFCF, 0x707070 },
                { QPalette::ButtonText, 0xCFCFCF, 0xCFCFCF, 0xCFCFCF },
                { QPalette::Base, 0x4B4B51, 0x4B4B51, 0x4B4B51 },
                { QPalette::Window, 0x4D4D53, 0x4D4D53, 0x4D4D53 },
                { QPalette::Shadow, 0x767472, 0x767472, 0x767472 },
                { QPalette::Highlight, 0x59595E, 0x59595E, 0x59595E },
                { QPalette::Link, 0xFFFFFF, 0xFFFFFF, 0xFFFFFF },
                { QPalette::AlternateBase, 0x505056, 0x505056, 0x505056 },
                { QPalette::ToolTipBase, 0x45454B, 0x45454B, 0x45454B },
                { QPalette::ToolTipText, 0x999999, 0x999999, 0x999999 },
            }) {
                palette.setColor(QPalette::Active, s.role, s.a);
                palette.setColor(QPalette::Inactive, s.role, s.i);
                palette.setColor(QPalette::Disabled, s.role, s.d);
            }
    }
    qApp->setPalette(palette);
}

void MainWindow::openSessionDock()
{
    for (auto p = mSessionEditor->parentWidget(); p; p = p->parentWidget())
        p->setVisible(true);
}

void MainWindow::openMessageDock()
{
    for (auto p = mMessageWindow->parentWidget(); p; p = p->parentWidget())
        p->setVisible(true);
}

void MainWindow::populateSampleSessions()
{
    const auto paths = {
        QCoreApplication::applicationDirPath() + "/../share/gpupad/samples",
        QCoreApplication::applicationDirPath() + "/samples",
        QString("/usr/share/gpupad/samples"),
    };
    for (const auto &path : paths) {
        if (!mUi->menuSampleSessions->actions().empty())
            return;

        auto samples = QDir(path);
        if (samples.exists()) {
            samples.setFilter(QDir::Dirs | QDir::NoDotAndDotDot);
            auto entries = samples.entryInfoList();
            for (auto entry = entries.cbegin(); entry != entries.cend(); ++entry) {
                auto sample = QDir(entry->absoluteFilePath());
                sample.setNameFilters({ "*.gpjs" });
                auto sessions = sample.entryInfoList();
                if (!sessions.empty()) {
                    auto action = mUi->menuSampleSessions->addAction(
                        entry->fileName(), this, SLOT(openSampleSession()));
                    action->setData(sessions.first().absoluteFilePath());
                }
            }
        }
    }
}

void MainWindow::openSampleSession()
{
    if (auto action = qobject_cast<QAction*>(QObject::sender()))
        if (openSession(action->data().toString())) {
            mUi->actionEvalSteady->setChecked(true);
            mSessionEditor->activateFirstItem();
        }
}

void MainWindow::openOnlineHelp()
{
    QDesktopServices::openUrl(QUrl("https://github.com/houmaster/gpupad"));
}

void MainWindow::openAbout()
{
    const auto title = tr("About %1").arg(QApplication::applicationName());
    const auto text = QStringLiteral(
       "<h3>%1 %2</h3>"
       "%3<br>"
       "<a href='%4'>%4</a><br><br>"
       "Copyright &copy; 2016-2018<br>"
       "Albert Kalchmair<br>"
       "%5<br><br>"
       "%6<br>"
       "%7<br>")
       .arg(QApplication::applicationName())
       .arg(QApplication::applicationVersion())
       .arg(tr("A text editor for efficiently editing GLSL shaders of all kinds."))
       .arg("https://github.com/houmaster/gpupad")
       .arg(tr("All Rights Reserved."))
       .arg(tr("This program comes with absolutely no warranty."))
       .arg(tr("See the GNU General Public License, version 3 for details."));
    QMessageBox::about(this, title, text);
}
