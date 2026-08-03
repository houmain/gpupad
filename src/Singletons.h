#pragma once

#include <QString>
#include <memory>

struct AdapterIdentity;
class QMainWindow;
class Settings;
class FileCache;
class FileDialog;
class EditorManager;
class SynchronizeLogic;
class SessionModel;
class Renderer;
class MediaManager;
class InputState;
class CustomActions;
class ScriptEngine;

using RendererPtr = std::shared_ptr<Renderer>;

bool onMainThread();

class Singletons
{
public:
    static void selectAdapter(const AdapterIdentity &adapter,
        const QString &apiVersion);
    static const AdapterIdentity &selectedAdapter();
    static const QString& selectedApiVersion();
    static RendererPtr sessionRenderer();
    static RendererPtr glRenderer();
    static RendererPtr vkRenderer();
    static RendererPtr d3dRenderer();
    static Settings &settings();
    static FileCache &fileCache();
    static FileDialog &fileDialog();
    static EditorManager &editorManager();
    static SessionModel &sessionModel();
    static SynchronizeLogic &synchronizeLogic();
    static MediaManager &mediaManager();
    static InputState &inputState();
    static CustomActions &customActions();
    static ScriptEngine &defaultScriptEngine();

    explicit Singletons(QMainWindow *window);
    ~Singletons();

private:
    static Singletons *sInstance;

    std::unique_ptr<AdapterIdentity> mSelectedAdapter;
    QString mSelectedApiVersion;
    std::unique_ptr<Settings> mSettings;
    std::unique_ptr<FileCache> mFileCache;
    std::unique_ptr<FileDialog> mFileDialog;
    std::unique_ptr<EditorManager> mEditorManager;
    std::unique_ptr<SessionModel> mSessionModel;
    std::unique_ptr<SynchronizeLogic> mSynchronizeLogic;
    std::unique_ptr<MediaManager> mMediaManager;
    std::unique_ptr<InputState> mInputState;
    std::unique_ptr<CustomActions> mCustomActions;
    std::shared_ptr<ScriptEngine> mDefaultScriptEngine;
    std::shared_ptr<Renderer> mVKRenderer;
    std::shared_ptr<Renderer> mGLRenderer;
    std::shared_ptr<Renderer> mD3DRenderer;
};
