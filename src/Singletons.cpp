#include "Singletons.h"
#include "MessageWindow.h"
#include "files/SourceEditorSettings.h"
#include "files/FileDialog.h"
#include "files/FileManager.h"
#include "files/FindReplaceBar.h"
#include "SynchronizeLogic.h"
#include "session/SessionModel.h"
#include "render/Renderer.h"

Singletons *Singletons::sInstance;

Renderer &Singletons::renderer()
{
    return *sInstance->mRenderer;
}

MessageWindow &Singletons::messageWindow()
{
    return *sInstance->mMessageWindow;
}

SourceEditorSettings &Singletons::sourceEditorSettings()
{
    return *sInstance->mSourceEditorSettings;
}

FileDialog &Singletons::fileDialog()
{
    return *sInstance->mFileDialog;
}

FileManager &Singletons::fileManager()
{
    return *sInstance->mFileManager;
}

SessionModel &Singletons::sessionModel()
{
    return *sInstance->mSessionModel;
}

SynchronizeLogic &Singletons::synchronizeLogic()
{
    return *sInstance->mSynchronizeLogic;
}

FindReplaceBar &Singletons::findReplaceBar()
{
    return *sInstance->mFindReplaceBar;
}

Singletons::Singletons()
{
    sInstance = this;
    mRenderer.reset(new Renderer());
    mMessageWindow.reset(new MessageWindow());
    mSourceEditorSettings.reset(new SourceEditorSettings());
    mFileDialog.reset(new FileDialog());
    mFileManager.reset(new FileManager());
    mSessionModel.reset(new SessionModel());
    mSynchronizeLogic.reset(new SynchronizeLogic());
    mFindReplaceBar.reset(new FindReplaceBar());
}

Singletons::~Singletons() = default;
