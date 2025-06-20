#pragma once

#include "EditActions.h"
#include "Evaluation.h"
#include "session/Item.h"
#include <QMainWindow>

namespace Ui {
    class MainWindow;
}

class QSplitter;
class QToolBar;
class QToolButton;
class QLabel;
class Singletons;
class MessageWindow;
class OutputWindow;
class FileBrowserWindow;
class EditorManager;
class SessionEditor;
class PropertiesEditor;
class Theme;

class MainWindow final : public QMainWindow
{
    Q_OBJECT
public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow() override;
    bool hasEditor() const;

public Q_SLOTS:
    void newFile();
    void openFile();
    bool openFile(const QString &fileName, bool asBinaryFile = false);
    bool saveFile();
    bool saveFileAs();
    bool saveAllFiles();
    bool reloadFile();
    bool closeFile();
    bool closeAllFiles();
    void openContainingFolder();
    void openSessionDock();
    void openMessageDock();
    void openOnlineHelp();
    void populateThemesMenu();
    void populateSampleSessions();
    void setSelectedTheme();
    void openSampleSession();
    void openAbout();
    void ignoreNextAlt() { mIgnoreNextAlt = true; }

protected:
    void closeEvent(QCloseEvent *event) override;
    void dragEnterEvent(QDragEnterEvent *event) override;
    void dropEvent(QDropEvent *event) override;
    bool eventFilter(QObject *watched, QEvent *event) override;
    void keyPressEvent(QKeyEvent *event) override;
    void keyReleaseEvent(QKeyEvent *event) override;
    QMenu *createPopupMenu() override;

private:
    void writeSettings();
    void readSettings();
    void updateCurrentEditor();
    void disconnectEditActions();
    void connectEditActions();
    void updateFileActions();
    void focusNextEditor();
    void focusPreviousEditor();
    void setFullScreen(bool fullScreen);
    void stopSteadyEvaluation();
    void setEvaluationMode(EvaluationMode evaluationMode);
    void updateEvaluationMode();
    bool openSession(const QString &fileName);
    bool saveSession();
    bool saveSessionAs();
    bool copySessionFiles(const QString &fromPath, const QString &toPath);
    void saveSessionState(const QString &sessionFileName);
    bool restoreSessionState(const QString &sessionFileName);
    bool closeSession();
    bool reloadSession();
    void addToRecentFileList(QString fileName);
    void updateRecentFileActions();
    void openRecentFile();
    void updateCustomActionsMenu();
    void handleMessageActivated(ItemId itemId, QString fileName, int line,
        int column);
    void handleThemeChanging(const Theme &theme);
    void handleHideMenuBarChanged(bool hide);
    void setToolbarIconVisible(QAction *action, bool visible);

    Ui::MainWindow *mUi{};
    QToolButton *mMenuButton{};
    QSplitter *mSessionSplitter{};
    QToolBar *mFullScreenBar{};
    QLabel *mFullScreenTitle{};
    EditActions mEditActions;
    std::unique_ptr<MessageWindow> mMessageWindow;
    std::unique_ptr<Singletons> mSingletons;
    std::unique_ptr<OutputWindow> mOutputWindow;
    std::unique_ptr<FileBrowserWindow> mFileBrowserWindow;
    EditorManager &mEditorManager;
    std::unique_ptr<SessionEditor> mSessionEditor;
    QDockWidget *mSessionDock{};
    std::unique_ptr<PropertiesEditor> mPropertiesEditor;
    QList<QMetaObject::Connection> mConnectedEditActions;
    QStringList mRecentFiles;
    QList<QAction *> mRecentSessionActions;
    QList<QAction *> mRecentFileActions;
    bool mLastPressWasAlt{};
    bool mIgnoreNextAlt{};
};
