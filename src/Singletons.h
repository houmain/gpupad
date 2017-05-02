#ifndef SINGLETONS_H
#define SINGLETONS_H

#include <QScopedPointer>

class MessageWindow;
class SourceEditorSettings;
class FileDialog;
class FileManager;
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
    static FileDialog &fileDialog();
    static FileManager &fileManager();
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
    QScopedPointer<FileDialog> mFileDialog;
    QScopedPointer<FileManager> mFileManager;
    QScopedPointer<SessionModel> mSessionModel;
    QScopedPointer<SynchronizeLogic> mSynchronizeLogic;
    QScopedPointer<FindReplaceBar> mFindReplaceBar;
};

#endif // SINGLETONS_H
