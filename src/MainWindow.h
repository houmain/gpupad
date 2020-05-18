#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include "EditActions.h"
#include "session/Item.h"
#include <QMainWindow>

namespace Ui {
class MainWindow;
}

class QSplitter;
class Singletons;
class MessageWindow;
class OutputWindow;
class EditorManager;
class SessionEditor;
class SessionProperties;
class CustomActions;

class MainWindow : public QMainWindow
{
    Q_OBJECT
public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow() override;
    bool hasEditor() const;

public slots:
    void newFile();
    void openFile();
    bool openFile(const QString &fileName,
        bool asBinaryFile = false);
    bool saveFile();
    bool saveFileAs();
    bool saveAllFiles();
    bool reloadFile();
    bool closeFile();
    void openSessionDock();
    void openMessageDock();
    void openOnlineHelp();
    void populateSampleSessions();
    void openSampleSession();
    void openAbout();

protected:
    void closeEvent(QCloseEvent *event) override;

private:
    void writeSettings();
    void readSettings();
    void updateCurrentEditor();
    void disconnectEditActions();
    void connectEditActions();
    void updateFileActions();
    void focusNextEditor();
    void focusPreviousEditor();
    void stopEvaluation();
    void updateEvaluationMode();
    bool openSession(const QString &fileName);
    bool saveSession();
    bool saveSessionAs();
    bool copySessionFiles(const QString &fromPath, const QString &toPath);
    bool closeSession();
    void addToRecentFileList(QString fileName);
    void updateRecentFileActions();
    void openRecentFile();
    void updateCustomActionsMenu();
    void handleMessageActivated(ItemId itemId,
        QString fileName, int line, int column);
    void handleDarkThemeChanging(bool enabled);

    Ui::MainWindow *mUi{ };
    QSplitter *mSessionSplitter{ };
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
    QList<QAction*> mRecentFileActions;
};

#endif // MAINWINDOW_H
