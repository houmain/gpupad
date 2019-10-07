#ifndef SINGLETONS_H
#define SINGLETONS_H

#include <QScopedPointer>

class QMainWindow;
class Settings;
class FileCache;
class FileDialog;
class EditorManager;
class SynchronizeLogic;
class SessionModel;
class Renderer;
class GLShareSynchronizer;

bool onMainThread();

class Singletons
{
public:
    static Renderer &renderer();
    static Settings &settings();
    static FileCache &fileCache();
    static FileDialog &fileDialog();
    static EditorManager &editorManager();
    static SessionModel &sessionModel();
    static SynchronizeLogic &synchronizeLogic();
    static GLShareSynchronizer &glShareSynchronizer();

    explicit Singletons(QMainWindow *window);
    ~Singletons();

private:
    static Singletons *sInstance;

    QScopedPointer<Renderer> mRenderer;
    QScopedPointer<Settings> mSettings;
    QScopedPointer<FileCache> mFileCache;
    QScopedPointer<FileDialog> mFileDialog;
    QScopedPointer<EditorManager> mEditorManager;
    QScopedPointer<SessionModel> mSessionModel;
    QScopedPointer<SynchronizeLogic> mSynchronizeLogic;
    QScopedPointer<GLShareSynchronizer> mGLShareSynchronizer;
};

#endif // SINGLETONS_H
