#include "MainWindow.h"
#include "ui_MainWindow.h"
#include "session/SessionEditor.h"
#include "session/SessionProperties.h"
#include "Singletons.h"
#include "MessageWindow.h"
#include "SynchronizeLogic.h"
#include "editors/EditorManager.h"
#include "editors/FindReplaceBar.h"
#include "editors/SourceEditorSettings.h"
#include <QCloseEvent>
#include <QMessageBox>
#include <QDockWidget>
#include <QDesktopServices>
#include <QSplitter>
#include <QActionGroup>
#include <QFontDialog>

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
    , mSingletons(new Singletons())
    , mSessionEditor(new SessionEditor())
    , mSessionProperties(new SessionProperties())
    , mEditorManager(Singletons::editorManager())
{
    mUi->setupUi(this);

    // TODO: why do have spinboxes an extra margin on my machine? broken theme?
    setStyleSheet("QSpinBox { margin-bottom:-1px; }");

    auto icon = QIcon(":images/16x16/icon.png");
    icon.addFile(":images/32x32/icon.png");
    setWindowIcon(icon);
    setContentsMargins(2, 0, 2, 0);

    mUi->menuView->addAction(mUi->mainToolBar->toggleViewAction());

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

    auto splitter = new AutoOrientationSplitter(this);
    splitter->addWidget(mSessionEditor.data());
    splitter->addWidget(mSessionProperties.data());

    dock = new QDockWidget(tr("Session"), this);
    dock->setObjectName("Session");
    dock->setFeatures(QDockWidget::DockWidgetClosable |
                      QDockWidget::DockWidgetMovable);
    dock->setWidget(splitter);
    mUi->menuView->addAction(dock->toggleViewAction());
    addDockWidget(Qt::LeftDockWidgetArea, dock);

    dock = new QDockWidget(tr("Messages"), this);
    dock->setObjectName("Messages");
    dock->setFeatures(QDockWidget::DockWidgetClosable |
                      QDockWidget::DockWidgetMovable);
    dock->setWidget(&Singletons::messageWindow());
    mUi->menuView->addAction(dock->toggleViewAction());
    addDockWidget(Qt::RightDockWidgetArea, dock);

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
    mUi->actionReload->setShortcuts(QKeySequence::Refresh);
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
        mUi->actionFindReplace
    };

    readSettings();

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
    connect(mUi->actionPreferences, &QAction::triggered,
        this, &MainWindow::openPreferences);
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

    auto& messageWindow = Singletons::messageWindow();
    auto& synchronizeLogic = Singletons::synchronizeLogic();
    connect(mSessionEditor.data(), &SessionEditor::itemActivated,
        &synchronizeLogic, &SynchronizeLogic::handleItemActivated);
    connect(&mEditorManager, &EditorManager::editorRenamed,
        &synchronizeLogic, &SynchronizeLogic::handleFileRenamed);
    connect(&mEditorManager, &EditorManager::sourceEditorChanged,
        &synchronizeLogic, &SynchronizeLogic::handleSourceEditorChanged);
    connect(&mEditorManager, &EditorManager::binaryEditorChanged,
        &synchronizeLogic, &SynchronizeLogic::handleBinaryEditorChanged);
    connect(&mEditorManager, &EditorManager::imageEditorChanged,
        &synchronizeLogic, &SynchronizeLogic::handleImageEditorChanged);
    connect(&messageWindow, &MessageWindow::messageActivated,
        &synchronizeLogic, &SynchronizeLogic::handleMessageActivated);

    auto& sourceEditorSettings = Singletons::sourceEditorSettings();
    connect(mUi->actionSelectFont, &QAction::triggered,
        this, &MainWindow::selectFont);
    connect(mUi->actionAutoIndentation, &QAction::triggered,
        &sourceEditorSettings, &SourceEditorSettings::setAutoIndentation);
    connect(mUi->actionLineWrapping, &QAction::triggered,
        &sourceEditorSettings, &SourceEditorSettings::setLineWrap);
    connect(mUi->actionIndentWithSpaces, &QAction::triggered,
        &sourceEditorSettings, &SourceEditorSettings::setIndentWithSpaces);
    auto indentActionGroup = new QActionGroup(this);
    connect(indentActionGroup, &QActionGroup::triggered,
        [this](QAction* action) { setTabSize(action->text().toInt()); });
    for (auto i = 1; i <= 8; i++) {
        auto action = new QAction(QString::number(i));
        mUi->menuTabSize->addAction(action);
        action->setCheckable(true);
        action->setChecked(i == sourceEditorSettings.tabSize());
        action->setActionGroup(indentActionGroup);
    }

    mUi->actionIndentWithSpaces->setChecked(sourceEditorSettings.indentWithSpaces());
    mUi->actionAutoIndentation->setChecked(sourceEditorSettings.autoIndentation());
    mUi->actionLineWrapping->setChecked(sourceEditorSettings.lineWrap());

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
    mSettings.beginGroup("General");
    if (!isMaximized())
        mSettings.setValue("geometry", saveGeometry());
    mSettings.setValue("maximized", isMaximized());
    mSettings.setValue("state", saveState());

    auto& editorSettings = Singletons::sourceEditorSettings();
    mSettings.setValue("tabSize", editorSettings.tabSize());
    mSettings.setValue("lineWrap", editorSettings.lineWrap());
    mSettings.setValue("indentWithSpaces", editorSettings.indentWithSpaces());
    mSettings.setValue("autoIndentation", editorSettings.autoIndentation());
    mSettings.setValue("font", editorSettings.font().toString());

    auto& fileDialog = Singletons::fileDialog();
    mSettings.setValue("lastDirectory", fileDialog.directory().absolutePath());

    mSettings.endGroup();
}

void MainWindow::readSettings()
{
    mSettings.beginGroup("General");
    resize(800, 600);
    restoreGeometry(mSettings.value("geometry").toByteArray());
    if (mSettings.value("maximized").toBool())
        setWindowState(Qt::WindowMaximized);
    restoreState(mSettings.value("state").toByteArray());

    auto& editorSettings = Singletons::sourceEditorSettings();
    editorSettings.setTabSize(mSettings.value("tabSize", "4").toInt());
    editorSettings.setLineWrap(mSettings.value("lineWrap", "false").toBool());
    editorSettings.setIndentWithSpaces(
        mSettings.value("indentWithSpaces", "false").toBool());
    editorSettings.setAutoIndentation(
        mSettings.value("autoIndentation", "true").toBool());

    auto fontSettings = mSettings.value("font").toString();
    if (!fontSettings.isEmpty()) {
        auto font = QFont();
        if (font.fromString(fontSettings))
            editorSettings.setFont(font);
    }

    auto& fileDialog = Singletons::fileDialog();
    fileDialog.setDirectory(mSettings.value("lastDirectory").toString());

    mSettings.endGroup();
}

void MainWindow::closeEvent(QCloseEvent *event)
{
    if (closeAllFiles() && closeSession())
        event->accept();
    else
        event->ignore();
}

void MainWindow::updateCurrentEditor()
{
    if (!qApp->focusWidget())
        return;

    if (!mEditorManager.updateCurrentEditor())
        return;

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
        mEditActions.selectAll, mEditActions.findReplace
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

void MainWindow::newFile()
{
    mEditorManager.openNewSourceEditor();
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
    Singletons::synchronizeLogic().deactivateCalls();

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

void MainWindow::openSessionDock()
{
    for (auto p = mSessionEditor->parentWidget(); p; p = p->parentWidget())
        p->setVisible(true);
}

void MainWindow::openMessageDock()
{
    auto& messageWindow = Singletons::messageWindow();
    for (auto p = messageWindow.parentWidget(); p; p = p->parentWidget())
        p->setVisible(true);
}

void MainWindow::openPreferences()
{
}

void MainWindow::selectFont()
{
    auto font = Singletons::sourceEditorSettings().font();
    QFontDialog dialog{ font };
    connect(&dialog, &QFontDialog::currentFontChanged,
        &Singletons::sourceEditorSettings(),
        &SourceEditorSettings::setFont);
    if (dialog.exec() == QDialog::Accepted)
        font = dialog.selectedFont();
    Singletons::sourceEditorSettings().setFont(font);
}

void MainWindow::setTabSize(int tabSize)
{
    Singletons::sourceEditorSettings().setTabSize(tabSize);
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
