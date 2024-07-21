#include "Singletons.h"
#include "EvaluatedPropertyCache.h"
#include "FileCache.h"
#include "FileDialog.h"
#include "InputState.h"
#include "Settings.h"
#include "SynchronizeLogic.h"
#include "VideoManager.h"
#include "editors/EditorManager.h"
#include "render/opengl/GLRenderer.h"
#include "render/opengl/GLShareSynchronizer.h"
#include "render/vulkan/VKRenderer.h"
#include "session/SessionModel.h"
#include <QApplication>

Singletons *Singletons::sInstance;

bool onMainThread()
{
    return (QThread::currentThread() == QApplication::instance()->thread());
}

void Singletons::resetRenderer(RenderAPI api)
{
    Q_ASSERT(onMainThread());
    sInstance->mRenderApi = api;
}

RendererPtr Singletons::renderer()
{
    Q_ASSERT(onMainThread());
    return (sInstance->mRenderApi == RenderAPI::Vulkan ? vkRenderer()
                                                       : glRenderer());
}

RendererPtr Singletons::glRenderer()
{
    Q_ASSERT(onMainThread());
    if (!sInstance->mGLRenderer)
        sInstance->mGLRenderer.reset(new GLRenderer());
    return sInstance->mGLRenderer;
}

RendererPtr Singletons::vkRenderer()
{
    Q_ASSERT(onMainThread());
    if (!sInstance->mVKRenderer)
        sInstance->mVKRenderer.reset(new VKRenderer());
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

GLShareSynchronizer &Singletons::glShareSynchronizer()
{
    return *sInstance->mGLShareSynchronizer;
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

EvaluatedPropertyCache &Singletons::evaluatedPropertyCache()
{
    return *sInstance->mEvaluatedPropertyCache;
}

Singletons::Singletons(QMainWindow *window)
    : mSettings(new Settings())
    , mFileCache(new FileCache())
    , mFileDialog(new FileDialog(window))
    , mEditorManager(new EditorManager())
    , mSessionModel(new SessionModel())
    , mGLShareSynchronizer(new GLShareSynchronizer())
    , mVideoManager(new VideoManager())
    , mInputState(new InputState())
    , mEvaluatedPropertyCache(new EvaluatedPropertyCache())
{
    Q_ASSERT(onMainThread());
    sInstance = this;
    mSynchronizeLogic.reset(new SynchronizeLogic());

    QObject::connect(&fileCache(), &FileCache::videoPlayerRequested,
        &videoManager(), &VideoManager::handleVideoPlayerRequested,
        Qt::QueuedConnection);
}

Singletons::~Singletons() = default;
