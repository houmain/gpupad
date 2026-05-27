#include "Singletons.h"
#include "FileCache.h"
#include "FileDialog.h"
#include "InputState.h"
#include "Settings.h"
#include "SynchronizeLogic.h"
#include "VideoManager.h"
#include "editors/EditorManager.h"
#include "render/AdapterIdentity.h"
#include "render/Renderer.h"
#if defined(_WIN32)
#  include "render/direct3d/D3DDevice.h"
#endif
#if defined(OPENGL_ENABLED)
#  include "render/opengl/GLDevice.h"
#endif
#if defined(VULKAN_ENABLED)
#  include "render/vulkan/VKDevice.h"
#endif
#include "session/SessionModel.h"
#include "scripting/CustomActions.h"
#include "scripting/ScriptEngine.h"
#include <QApplication>

Singletons *Singletons::sInstance;

bool onMainThread()
{
    return (QThread::currentThread() == QApplication::instance()->thread());
}

void Singletons::selectAdapter(const AdapterIdentity &adapter)
{
    Q_ASSERT(onMainThread());
    if (sInstance->mSelectedAdapter && *sInstance->mSelectedAdapter == adapter)
        return;

    sInstance->mSynchronizeLogic->resetRenderSession();
    sInstance->mGLRenderer.reset();
    sInstance->mVKRenderer.reset();
    sInstance->mD3DRenderer.reset();
    sInstance->mSelectedAdapter = std::make_unique<AdapterIdentity>(adapter);
#if defined(VULKAN_ENABLED)
    resetSharedVKDevice();
#endif
    sInstance->mEditorManager->recreateTextureEditorGpuWindows();
}

const AdapterIdentity &Singletons::selectedAdapter()
{
    Q_ASSERT(onMainThread());
    static const auto sEmpty = AdapterIdentity{};
    return (sInstance->mSelectedAdapter ? *sInstance->mSelectedAdapter
                                        : sEmpty);
}

RendererPtr Singletons::sessionRenderer()
{
    Q_ASSERT(onMainThread());

    // reset failed renderers so error message disappears
    const auto type = sessionModel().sessionItem().renderer;
    for (auto renderer_ptr : {
             &sInstance->mGLRenderer,
             &sInstance->mVKRenderer,
             &sInstance->mD3DRenderer,
         })
        if (auto renderer = *renderer_ptr)
            if (renderer->failed() && renderer->type() != type)
                *renderer_ptr = nullptr;

    switch (type) {
    case Session::Renderer::OpenGL:   return glRenderer();
    case Session::Renderer::Vulkan:   return vkRenderer();
    case Session::Renderer::Direct3D: return d3dRenderer();
    }
    Q_UNREACHABLE();
    return nullptr;
}

RendererPtr Singletons::glRenderer()
{
    Q_ASSERT(onMainThread());
    if (!sInstance->mGLRenderer) {
#if defined(OPENGL_ENABLED)
        sInstance->mGLRenderer =
            std::make_shared<Renderer>(Renderer::Type::OpenGL,
                std::make_unique<GLDevice>(), selectedAdapter());
#else
        sInstance->mGLRenderer = std::make_shared<Renderer>(
            Renderer::Type::OpenGL, MessageType::OpenGLVersionNotAvailable);
#endif
    }
    return sInstance->mGLRenderer;
}

RendererPtr Singletons::vkRenderer()
{
    Q_ASSERT(onMainThread());
    if (!sInstance->mVKRenderer) {
#if defined(VULKAN_ENABLED)
        sInstance->mVKRenderer =
            std::make_shared<Renderer>(Renderer::Type::Vulkan,
                std::make_unique<VKDevice>(), selectedAdapter());
#else
        sInstance->mVKRenderer = std::make_shared<Renderer>(
            Renderer::Type::Vulkan, MessageType::VulkanNotAvailable);
#endif
    }
    return sInstance->mVKRenderer;
}

RendererPtr Singletons::d3dRenderer()
{
    Q_ASSERT(onMainThread());
    if (!sInstance->mD3DRenderer) {
#if defined(_WIN32)
        sInstance->mD3DRenderer =
            std::make_shared<Renderer>(Renderer::Type::Direct3D,
                std::make_unique<D3DDevice>(), selectedAdapter());
#else
        sInstance->mD3DRenderer = std::make_shared<Renderer>(
            Renderer::Type::Direct3D, MessageType::Direct3DNotAvailable);
#endif
    }
    return sInstance->mD3DRenderer;
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
    return *sInstance->mCustomActions;
}

ScriptEngine &Singletons::defaultScriptEngine()
{
    Q_ASSERT(onMainThread());
    return *sInstance->mDefaultScriptEngine;
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

    mDefaultScriptEngine = ScriptEngine::make(QDir::current());
}

Singletons::~Singletons() = default;
