#include "EditorScriptObject.h"
#include "AppScriptObject.h"
#include "Singletons.h"
#include "editors/EditorManager.h"
#include <QJsonArray>

EditorScriptObject::EditorScriptObject(AppScriptObject *appScriptObject,
    const QString &fileName, QSize viewportSize)
    : QObject(appScriptObject)
    , mAppScriptObject(appScriptObject)
    , mFileName(fileName)
    , mViewportSize(viewportSize)
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
    const auto viewportSize =
        Singletons::editorManager().getViewportSize(mFileName);
    if (mViewportSize != viewportSize) {
        mViewportSize = viewportSize;
        Q_EMIT viewportResized();
    }
}

QJsonValue EditorScriptObject::viewportSize() const
{
    mViewportSizeWasRead = true;
    return QJsonArray({ mViewportSize.width(), mViewportSize.height() });
}
