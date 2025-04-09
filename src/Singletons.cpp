#include "Singletons.h"
#include "FileCache.h"
#include "FileDialog.h"
#include "InputState.h"
#include "Settings.h"
#include "SynchronizeLogic.h"
#include "VideoManager.h"
#include "editors/EditorManager.h"
#include "render/opengl/GLRenderer.h"
#include "render/vulkan/VKRenderer.h"
#include "session/SessionModel.h"
#include "scripting/CustomActions.h"
#include <QApplication>

Singletons *Singletons::sInstance;

bool onMainThread()
{
    return (QThread::currentThread() == QApplication::instance()->thread());
}

RendererPtr Singletons::sessionRenderer()
{
    Q_ASSERT(onMainThread());
    const auto &renderer = sessionModel().sessionItem().renderer;
    return (renderer == "Vulkan" ? vkRenderer() : glRenderer());
}

RendererPtr Singletons::glRenderer()
{
    Q_ASSERT(onMainThread());
    if (!sInstance->mGLRenderer)
        sInstance->mGLRenderer = std::make_shared<GLRenderer>();
    return sInstance->mGLRenderer;
}

RendererPtr Singletons::vkRenderer()
{
    Q_ASSERT(onMainThread());
    if (!sInstance->mVKRenderer)
        sInstance->mVKRenderer = std::make_shared<VKRenderer>();
    return sInstance->mVKRenderer;
}

Settings &Singletons::settings()
{
    Q_ASSERT(onMainThread());
    return *sInstance->mSettings;
}

FileCache &Singletons::fileCache()
{
    return *sInstance->mFileCache;
}

FileDialog &Singletons::fileDialog()
{
    Q_ASSERT(onMainThread());
    return *sInstance->mFileDialog;
}

EditorManager &Singletons::editorManager()
{
    Q_ASSERT(onMainThread());
    return *sInstance->mEditorManager;
}

SessionModel &Singletons::sessionModel()
{
    Q_ASSERT(onMainThread());
    return *sInstance->mSessionModel;
}

SynchronizeLogic &Singletons::synchronizeLogic()
{
    Q_ASSERT(onMainThread());
    return *sInstance->mSynchronizeLogic;
}

VideoManager &Singletons::videoManager()
{
    Q_ASSERT(onMainThread());
    return *sInstance->mVideoManager;
}

InputState &Singletons::inputState()
{
    Q_ASSERT(onMainThread());
    return *sInstance->mInputState;
}

CustomActions &Singletons::customActions()
{
    Q_ASSERT(onMainThread());
    return *sInstance->mCustomActions;
}

Singletons::Singletons(QMainWindow *window)
    : mSettings(std::make_unique<Settings>())
    , mFileCache(std::make_unique<FileCache>())
    , mFileDialog(std::make_unique<FileDialog>(window))
    , mEditorManager(std::make_unique<EditorManager>())
    , mSessionModel(std::make_unique<SessionModel>())
    , mVideoManager(std::make_unique<VideoManager>())
    , mInputState(std::make_unique<InputState>())
    , mCustomActions(std::make_unique<CustomActions>())
{
    Q_ASSERT(onMainThread());
    sInstance = this;
    mSynchronizeLogic = std::make_unique<SynchronizeLogic>();

    QObject::connect(&fileCache(), &FileCache::videoPlayerRequested,
        &videoManager(), &VideoManager::handleVideoPlayerRequested,
        Qt::QueuedConnection);
}

Singletons::~Singletons() = default;
