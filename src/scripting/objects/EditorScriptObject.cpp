#include "EditorScriptObject.h"
#include "AppScriptObject.h"
#include "Singletons.h"
#include "editors/EditorManager.h"
#include <QJsonArray>

EditorScriptObject::EditorScriptObject(AppScriptObject *appScriptObject,
    const QString &fileName)
    : QObject(appScriptObject)
    , mAppScriptObject(appScriptObject)
    , mFileName(fileName)
{
}

EditorScriptObject::~EditorScriptObject()
{
    if (mAppScriptObject)
        mAppScriptObject->deregisterEditorScriptObject(this);
}

void EditorScriptObject::resetAppScriptObject()
{
    mAppScriptObject = nullptr;
}

void EditorScriptObject::update()
{
    mViewportSizeWasRead = false;
    mViewportSize = Singletons::editorManager().getViewportSize(mFileName);
}

QJsonValue EditorScriptObject::viewportSize() const
{
    mViewportSizeWasRead = true;
    return QJsonArray({ mViewportSize.width(), mViewportSize.height() });
}
