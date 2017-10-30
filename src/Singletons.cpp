#include "Singletons.h"
#include "MessageList.h"
#include "FileCache.h"
#include "FileDialog.h"
#include "SynchronizeLogic.h"
#include "Settings.h"
#include "editors/EditorManager.h"
#include "session/SessionModel.h"
#include "render/Renderer.h"
#include "scripting/CustomActions.h"
#include <QApplication>

Singletons *Singletons::sInstance;


bool onMainThread()
{
    return (QThread::currentThread() ==
        QApplication::instance()->thread());
}

Renderer &Singletons::renderer()
{
    Q_ASSERT(onMainThread());
    if (!sInstance->mRenderer)
        sInstance->mRenderer.reset(new Renderer());
    return *sInstance->mRenderer;
}

MessageList &Singletons::messageList()
{
    return *sInstance->mMessageList;
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

CustomActions &Singletons::customActions()
{
    Q_ASSERT(onMainThread());
    if (!sInstance->mCustomActions)
        sInstance->mCustomActions.reset(
            new CustomActions(sInstance->mMainWindow));
    return *sInstance->mCustomActions;
}

Singletons::Singletons(QMainWindow *window)
    : mMainWindow(window)
    , mMessageList(new MessageList())
    , mSettings(new Settings())
    , mFileCache(new FileCache())
    , mFileDialog(new FileDialog(window))
    , mEditorManager(new EditorManager())
    , mSessionModel(new SessionModel())
{
    Q_ASSERT(onMainThread());
    sInstance = this;
    mSynchronizeLogic.reset(new SynchronizeLogic());
}

Singletons::~Singletons() = default;
