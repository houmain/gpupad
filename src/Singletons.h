#ifndef SINGLETONS_H
#define SINGLETONS_H

#include <QScopedPointer>

class MessageWindow;
class SourceEditorSettings;
class FileCache;
class FileDialog;
class EditorManager;
class SynchronizeLogic;
class SessionModel;
class Renderer;
class FindReplaceBar;

class Singletons
{
public:
    static Renderer &renderer();
    static MessageWindow &messageWindow();
    static SourceEditorSettings &sourceEditorSettings();
    static FileCache& fileCache();
    static FileDialog &fileDialog();
    static EditorManager &editorManager();
    static SessionModel &sessionModel();
    static SynchronizeLogic &synchronizeLogic();
    static FindReplaceBar &findReplaceBar();

    explicit Singletons();
    ~Singletons();

private:
    static Singletons *sInstance;

    QScopedPointer<Renderer> mRenderer;
    QScopedPointer<MessageWindow> mMessageWindow;
    QScopedPointer<SourceEditorSettings> mSourceEditorSettings;
    QScopedPointer<FileCache> mFileCache;
    QScopedPointer<FileDialog> mFileDialog;
    QScopedPointer<EditorManager> mEditorManager;
    QScopedPointer<SessionModel> mSessionModel;
    QScopedPointer<SynchronizeLogic> mSynchronizeLogic;
    QScopedPointer<FindReplaceBar> mFindReplaceBar;
};

#endif // SINGLETONS_H
