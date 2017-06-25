#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include "EditActions.h"
#include <QMainWindow>

namespace Ui {
class MainWindow;
}

class QSplitter;
class Singletons;
class EditorManager;
class SessionEditor;
class SessionProperties;

class MainWindow : public QMainWindow
{
    Q_OBJECT
public:
    explicit MainWindow(QWidget *parent = 0);
    ~MainWindow();

public slots:
    void newFile();
    void openFile();
    void openFile(const QString &fileName);
    bool saveFile();
    bool saveFileAs();
    bool saveAllFiles();
    bool reloadFile();
    bool closeFile();
    bool closeAllFiles();
    void openSessionDock();
    void openMessageDock();
    void openDocumentation();
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
    void updateEvaluationMode();
    bool openSession(const QString &fileName);
    bool saveSession();
    bool saveSessionAs();
    bool closeSession();

    Ui::MainWindow *mUi{ };
    QSplitter *mSessionSplitter{ };
    EditActions mEditActions;
    QScopedPointer<Singletons> mSingletons;
    QScopedPointer<SessionEditor> mSessionEditor;
    QScopedPointer<SessionProperties> mSessionProperties;
    EditorManager &mEditorManager;
    QList<QMetaObject::Connection> mConnectedEditActions;
};

#endif // MAINWINDOW_H
