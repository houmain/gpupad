#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include "EditActions.h"
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
class EditorManager;
class SessionEditor;
class SessionProperties;
class CustomActions;

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
    bool openFile(const QString &fileName,
        bool asBinaryFile = false);
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
    void populateSampleSessions();
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
    void stopEvaluation();
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
    void handleMessageActivated(ItemId itemId,
        QString fileName, int line, int column);
    void handleDarkThemeChanging(bool enabled);
    void handleHideMenuBarChanged(bool hide);

    Ui::MainWindow *mUi{ };
    QToolButton* mMenuButton{ };
    QSplitter *mSessionSplitter{ };
    QToolBar* mFullScreenBar{ };
    QLabel* mFullScreenTitle{ };
    EditActions mEditActions;
    QScopedPointer<MessageWindow> mMessageWindow;
    QScopedPointer<CustomActions> mCustomActions;
    QScopedPointer<Singletons> mSingletons;
    QScopedPointer<OutputWindow> mOutputWindow;
    EditorManager &mEditorManager;
    QScopedPointer<SessionEditor> mSessionEditor;
    QDockWidget *mSessionDock{ };
    QScopedPointer<SessionProperties> mSessionProperties;
    QList<QMetaObject::Connection> mConnectedEditActions;
    QStringList mRecentFiles;
    QList<QAction*> mRecentSessionActions;
    QList<QAction*> mRecentFileActions;
    bool mLastPressWasAlt{ };
    bool mIgnoreNextAlt{ };
};

#endif // MAINWINDOW_H
