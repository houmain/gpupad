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
class EditorManager;
class SessionEditor;
class SessionProperties;

class MainWindow : public QMainWindow
{
    Q_OBJECT
public:
    explicit MainWindow(QWidget *parent = 0);
    ~MainWindow();
    bool hasEditor() const;

public slots:
    void newFile();
    void openFile();
    void openFile(const QString &fileName);
    bool saveFile();
    bool saveFileAs();
    bool saveAllFiles();
    bool reloadFile();
    bool closeFile();
    void openSessionDock();
    void openMessageDock();
    void openDocumentation();
    void openAbout();

private slots:
    void handleMessageActivated(ItemId itemId,
        QString fileName, int line, int column);

protected:
    void closeEvent(QCloseEvent *event) override;

private:
    void writeSettings();
    void readSettings();
    void updateCurrentEditor();
    void disconnectEditActions();
    void connectEditActions();
    void updateFileActions();
    void stopEvaluation();
    void updateEvaluationMode();
    void updateEvaluationInterval();
    bool openSession(const QString &fileName);
    bool saveSession();
    bool saveSessionAs();
    bool closeSession();
    void addToRecentFileList(const QString &fileName);
    void updateRecentFileActions();
    void openRecentFile();
    void updateCustomActionsMenu();

    Ui::MainWindow *mUi{ };
    QSplitter *mSessionSplitter{ };
    EditActions mEditActions;
    QScopedPointer<MessageWindow> mMessageWindow;
    QScopedPointer<Singletons> mSingletons;
    EditorManager &mEditorManager;
    QScopedPointer<SessionEditor> mSessionEditor;
    QScopedPointer<SessionProperties> mSessionProperties;
    QList<QMetaObject::Connection> mConnectedEditActions;
    QStringList mRecentFiles;
    QList<QAction*> mRecentFileActions;
};

#endif // MAINWINDOW_H
