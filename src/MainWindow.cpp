#include "MainWindow.h"
#include "ui_MainWindow.h"
#include "session/SessionEditor.h"
#include "session/SessionProperties.h"
#include "Singletons.h"
#include "MessageWindow.h"
#include "MessageList.h"
#include "Settings.h"
#include "SynchronizeLogic.h"
#include "editors/EditorManager.h"
#include "editors/FindReplaceBar.h"
#include <QCloseEvent>
#include <QMessageBox>
#include <QDockWidget>
#include <QDesktopServices>
#include <QSplitter>
#include <QActionGroup>

class AutoOrientationSplitter : public QSplitter
{
public:
    AutoOrientationSplitter(QWidget *parent) : QSplitter(parent)
    {
        setFrameShape(QFrame::StyledPanel);
        setChildrenCollapsible(false);
    }

    void resizeEvent(QResizeEvent *event) override
    {
        setOrientation(2 * width() > 3 * height() ?
            Qt::Horizontal : Qt::Vertical);

        const auto vertical = (orientation() == Qt::Vertical);
        setStretchFactor(vertical ? 1 : 0, 0);
        setStretchFactor(vertical ? 0 : 1, 100);

        QSplitter::resizeEvent(event);
    }
};

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , mUi(new Ui::MainWindow)
    , mMessageWindow(new MessageWindow())
    , mSingletons(new Singletons(this))
    , mEditorManager(Singletons::editorManager())
    , mSessionEditor(new SessionEditor())
    , mSessionProperties(new SessionProperties())
{
    mUi->setupUi(this);

    auto icon = QIcon(":images/16x16/icon.png");
    icon.addFile(":images/32x32/icon.png");
    setWindowIcon(icon);
    setContentsMargins(2, 0, 2, 0);

    takeCentralWidget();

    auto content = new QWidget(this);
    mEditorManager.setParent(content);
    Singletons::findReplaceBar().setParent(content);
    auto layout = new QVBoxLayout(content);
    layout->setMargin(0);
    layout->setSpacing(0);
    layout->addWidget(&mEditorManager);
    layout->addWidget(&Singletons::findReplaceBar());

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
    mUi->menuView->addAction(dock->toggleViewAction());
    addDockWidget(Qt::LeftDockWidgetArea, dock);
    mSessionEditor->addItemActions(mUi->menuSession);

    dock = new QDockWidget(tr("Messages"), this);
    dock->setObjectName("Messages");
    dock->setFeatures(QDockWidget::DockWidgetClosable |
                      QDockWidget::DockWidgetMovable);
    dock->setWidget(mMessageWindow.data());
    mUi->menuView->addAction(dock->toggleViewAction());
    addDockWidget(Qt::RightDockWidgetArea, dock);

    mUi->menuView->addAction(mUi->mainToolBar->toggleViewAction());

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
    mUi->actionDocumentation->setShortcuts(QKeySequence::HelpContents);
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
        this, &MainWindow::closeAllFiles);
    connect(mUi->actionQuit, &QAction::triggered,
        this, &MainWindow::close);
    connect(mUi->actionDocumentation, &QAction::triggered,
        this, &MainWindow::openDocumentation);
    connect(mUi->actionAbout, &QAction::triggered,
        this, &MainWindow::openAbout);
    connect(windowFileName, &QAction::changed,
        this, &MainWindow::updateFileActions);
    connect(qApp, &QApplication::focusChanged,
        this, &MainWindow::updateCurrentEditor);
    connect(mSessionEditor->selectionModel(), &QItemSelectionModel::currentChanged,
        mSessionProperties.data(), &SessionProperties::setCurrentModelIndex);
    connect(mUi->menuSession, &QMenu::aboutToShow,
        mSessionEditor.data(), &SessionEditor::updateItemActions);

    auto& synchronizeLogic = Singletons::synchronizeLogic();
    connect(mSessionEditor.data(), &SessionEditor::itemActivated,
        &synchronizeLogic, &SynchronizeLogic::handleItemActivated);
    connect(&mEditorManager, &EditorManager::editorRenamed,
        &synchronizeLogic, &SynchronizeLogic::handleFileRenamed);
    connect(&mEditorManager, &EditorManager::sourceEditorChanged,
        &synchronizeLogic, &SynchronizeLogic::handleFileItemsChanged);
    connect(&mEditorManager, &EditorManager::binaryEditorChanged,
        &synchronizeLogic, &SynchronizeLogic::handleFileItemsChanged);
    connect(&mEditorManager, &EditorManager::imageEditorChanged,
        &synchronizeLogic, &SynchronizeLogic::handleFileItemsChanged);
    connect(mMessageWindow.data(), &MessageWindow::messageActivated,
        this, &MainWindow::handleMessageActivated);
    connect(mMessageWindow.data(), &MessageWindow::messagesAdded,
        this, &MainWindow::openMessageDock);

    auto& settings = Singletons::settings();
    connect(mUi->actionSelectFont, &QAction::triggered,
        &settings, &Settings::selectFont);
    connect(mUi->actionAutoIndentation, &QAction::triggered,
        &settings, &Settings::setAutoIndentation);
    connect(mUi->actionLineWrapping, &QAction::triggered,
        &settings, &Settings::setLineWrap);
    connect(mUi->actionIndentWithSpaces, &QAction::triggered,
        &settings, &Settings::setIndentWithSpaces);

    connect(mUi->actionEvalManual, &QAction::triggered,
        this, &MainWindow::updateEvaluationMode);
    connect(mUi->actionEvalAuto, &QAction::toggled,
        this, &MainWindow::updateEvaluationMode);
    connect(mUi->actionEvalSteady, &QAction::toggled,
        this, &MainWindow::updateEvaluationMode);

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
        auto action = new QAction(QString::number(i));
        mUi->menuTabSize->addAction(action);
        action->setCheckable(true);
        action->setChecked(i == settings.tabSize());
        action->setActionGroup(indentActionGroup);
    }

    readSettings();
    updateEvaluationInterval();

    if (!mEditorManager.hasCurrentEditor())
        newFile();
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
    auto& settings = Singletons::settings();
    if (!isMaximized())
        settings.setValue("geometry", saveGeometry());
    settings.setValue("maximized", isMaximized());
    settings.setValue("state", saveState());
    settings.setValue("sessionSplitter", mSessionSplitter->saveState());

    auto& fileDialog = Singletons::fileDialog();
    settings.setValue("lastDirectory", fileDialog.directory().absolutePath());
}

void MainWindow::readSettings()
{
    auto& settings = Singletons::settings();
    resize(800, 600);
    restoreGeometry(settings.value("geometry").toByteArray());
    if (settings.value("maximized").toBool())
        setWindowState(Qt::WindowMaximized);
    restoreState(settings.value("state").toByteArray());
    mSessionSplitter->restoreState(settings.value("sessionSplitter").toByteArray());

    auto& fileDialog = Singletons::fileDialog();
    fileDialog.setDirectory(settings.value("lastDirectory").toString());

    mUi->actionIndentWithSpaces->setChecked(settings.indentWithSpaces());
    mUi->actionAutoIndentation->setChecked(settings.autoIndentation());
    mUi->actionLineWrapping->setChecked(settings.lineWrap());
}

void MainWindow::closeEvent(QCloseEvent *event)
{
    stopEvaluation();

    if (closeAllFiles() && closeSession())
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
    foreach (QMetaObject::Connection connection, mConnectedEditActions)
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

void MainWindow::newFile()
{
    mEditorManager.openNewSourceEditor("");
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
    if (FileDialog::isSessionFileName(fileName))
        openSession(fileName);
    else
        mEditorManager.openEditor(fileName);
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

bool MainWindow::closeAllFiles()
{
    if (!mEditorManager.closeAllEditors())
        return false;

    return true;
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
    if (FileDialog::isUntitled(mSessionEditor->fileName()))
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

void MainWindow::handleMessageActivated(ItemId itemId, QString fileName,
        int line, int column)
{
    if (itemId)
        mSessionEditor->setCurrentItem(itemId);
    else if (!fileName.isEmpty())
        Singletons::editorManager().openSourceEditor(fileName, true, line, column);
    else
        openSessionDock();
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

void MainWindow::openDocumentation()
{
    QDesktopServices::openUrl(QUrl("http://qt.io/"));
}

void MainWindow::openAbout()
{
    QMessageBox::about(this,
        tr("About %1").arg(QApplication::applicationName()),
        tr("<h3>%1 %2</h3>"
           "A text editor for efficiently editing GLSL shaders of all kinds.<br><br>"
           "Copyright &copy; 2016-2017<br>"
           "Albert Kalchmair<br>"
           "All Rights Reserved.<br><br>"
           "<a href='%3'>%3</a><br>")
           .arg(QApplication::applicationName())
           .arg(QApplication::applicationVersion())
           .arg("https://github.com/houmaster/gpupad"));
}
