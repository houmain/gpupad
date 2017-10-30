#ifndef SINGLETONS_H
#define SINGLETONS_H

#include <QScopedPointer>

class QMainWindow;
class MessageList;
class Settings;
class FileCache;
class FileDialog;
class EditorManager;
class SynchronizeLogic;
class SessionModel;
class Renderer;
class CustomActions;

bool onMainThread();

class Singletons
{
public:
    static Renderer &renderer();
    static MessageList &messageList();
    static Settings &settings();
    static FileCache& fileCache();
    static FileDialog &fileDialog();
    static EditorManager &editorManager();
    static SessionModel &sessionModel();
    static SynchronizeLogic &synchronizeLogic();
    static CustomActions &customActions();

    explicit Singletons(QMainWindow *window);
    ~Singletons();

private:
    static Singletons *sInstance;

    QMainWindow *mMainWindow;
    QScopedPointer<Renderer> mRenderer;
    QScopedPointer<MessageList> mMessageList;
    QScopedPointer<Settings> mSettings;
    QScopedPointer<FileCache> mFileCache;
    QScopedPointer<FileDialog> mFileDialog;
    QScopedPointer<EditorManager> mEditorManager;
    QScopedPointer<SessionModel> mSessionModel;
    QScopedPointer<SynchronizeLogic> mSynchronizeLogic;
    QScopedPointer<CustomActions> mCustomActions;
};

#endif // SINGLETONS_H
