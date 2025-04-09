#pragma once

#include <QString>
#include <memory>

class QMainWindow;
class Settings;
class FileCache;
class FileDialog;
class EditorManager;
class SynchronizeLogic;
class SessionModel;
class Renderer;
class VideoManager;
class InputState;
class CustomActions;

using RendererPtr = std::shared_ptr<Renderer>;

bool onMainThread();

class Singletons
{
public:
    static RendererPtr sessionRenderer();
    static RendererPtr glRenderer();
    static RendererPtr vkRenderer();
    static Settings &settings();
    static FileCache &fileCache();
    static FileDialog &fileDialog();
    static EditorManager &editorManager();
    static SessionModel &sessionModel();
    static SynchronizeLogic &synchronizeLogic();
    static VideoManager &videoManager();
    static InputState &inputState();
    static CustomActions &customActions();

    explicit Singletons(QMainWindow *window);
    ~Singletons();

private:
    static Singletons *sInstance;

    std::shared_ptr<Renderer> mVKRenderer;
    std::shared_ptr<Renderer> mGLRenderer;
    std::unique_ptr<Settings> mSettings;
    std::unique_ptr<FileCache> mFileCache;
    std::unique_ptr<FileDialog> mFileDialog;
    std::unique_ptr<EditorManager> mEditorManager;
    std::unique_ptr<SessionModel> mSessionModel;
    std::unique_ptr<SynchronizeLogic> mSynchronizeLogic;
    std::unique_ptr<VideoManager> mVideoManager;
    std::unique_ptr<InputState> mInputState;
    std::unique_ptr<CustomActions> mCustomActions;
};
