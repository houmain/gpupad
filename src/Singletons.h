#ifndef SINGLETONS_H
#define SINGLETONS_H

#include <memory>

class QMainWindow;
class Settings;
class FileCache;
class FileDialog;
class EditorManager;
class SynchronizeLogic;
class SessionModel;
class Renderer;
class GLShareSynchronizer;
class VideoManager;
class InputState;
class EvaluatedPropertyCache;
enum class RenderAPI : int;

using RendererPtr = std::shared_ptr<Renderer>;

bool onMainThread();

class Singletons
{
public:
    static void resetRenderer(RenderAPI api);
    static RendererPtr renderer();
    static RendererPtr glRenderer();
    static RendererPtr vkRenderer();
    static Settings &settings();
    static FileCache &fileCache();
    static FileDialog &fileDialog();
    static EditorManager &editorManager();
    static SessionModel &sessionModel();
    static SynchronizeLogic &synchronizeLogic();
    static GLShareSynchronizer &glShareSynchronizer();
    static VideoManager &videoManager();
    static InputState &inputState();
    static EvaluatedPropertyCache &evaluatedPropertyCache();

    explicit Singletons(QMainWindow *window);
    ~Singletons();

private:
    static Singletons *sInstance;

    RenderAPI mRenderApi{ };
    std::shared_ptr<Renderer> mVKRenderer;
    std::shared_ptr<Renderer> mGLRenderer;
    std::unique_ptr<Settings> mSettings;
    std::unique_ptr<FileCache> mFileCache;
    std::unique_ptr<FileDialog> mFileDialog;
    std::unique_ptr<EditorManager> mEditorManager;
    std::unique_ptr<SessionModel> mSessionModel;
    std::unique_ptr<SynchronizeLogic> mSynchronizeLogic;
    std::unique_ptr<GLShareSynchronizer> mGLShareSynchronizer;
    std::unique_ptr<VideoManager> mVideoManager;
    std::unique_ptr<InputState> mInputState;
    std::unique_ptr<EvaluatedPropertyCache> mEvaluatedPropertyCache;
};

#endif // SINGLETONS_H
