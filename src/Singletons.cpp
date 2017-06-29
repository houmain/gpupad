#include "Singletons.h"
#include "MessageWindow.h"
#include "FileCache.h"
#include "FileDialog.h"
#include "SynchronizeLogic.h"
#include "Settings.h"
#include "editors/EditorManager.h"
#include "editors/FindReplaceBar.h"
#include "session/SessionModel.h"
#include "render/Renderer.h"
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
    return *sInstance->mRenderer;
}

MessageWindow &Singletons::messageWindow()
{
    Q_ASSERT(onMainThread());
    return *sInstance->mMessageWindow;
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

FindReplaceBar &Singletons::findReplaceBar()
{
    Q_ASSERT(onMainThread());
    return *sInstance->mFindReplaceBar;
}

Singletons::Singletons(QMainWindow *window)
{
    Q_ASSERT(onMainThread());
    sInstance = this;
    mRenderer.reset(new Renderer());
    mMessageWindow.reset(new MessageWindow());
    mSettings.reset(new Settings());
    mFileCache.reset(new FileCache());
    mFileDialog.reset(new FileDialog(window));
    mEditorManager.reset(new EditorManager());
    mSessionModel.reset(new SessionModel());
    mSynchronizeLogic.reset(new SynchronizeLogic());
    mFindReplaceBar.reset(new FindReplaceBar());
}

Singletons::~Singletons() = default;
