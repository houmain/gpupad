#ifndef SINGLETONS_H
#define SINGLETONS_H

#include <QScopedPointer>

class QMainWindow;
class MessageWindow;
class Settings;
class FileCache;
class FileDialog;
class EditorManager;
class SynchronizeLogic;
class SessionModel;
class Renderer;
class FindReplaceBar;

bool onMainThread();

class Singletons
{
public:
    static Renderer &renderer();
    static MessageWindow &messageWindow();
    static Settings &settings();
    static FileCache& fileCache();
    static FileDialog &fileDialog();
    static EditorManager &editorManager();
    static SessionModel &sessionModel();
    static SynchronizeLogic &synchronizeLogic();
    static FindReplaceBar &findReplaceBar();

    explicit Singletons(QMainWindow *window);
    ~Singletons();

private:
    static Singletons *sInstance;

    QScopedPointer<Renderer> mRenderer;
    QScopedPointer<MessageWindow> mMessageWindow;
    QScopedPointer<Settings> mSettings;
    QScopedPointer<FileCache> mFileCache;
    QScopedPointer<FileDialog> mFileDialog;
    QScopedPointer<EditorManager> mEditorManager;
    QScopedPointer<SessionModel> mSessionModel;
    QScopedPointer<SynchronizeLogic> mSynchronizeLogic;
    QScopedPointer<FindReplaceBar> mFindReplaceBar;
};

#endif // SINGLETONS_H
