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
    Q_ASSERT(onMainThread());

    switch (Singletons::editorManager().getEditorType(mFileName)) {
    case EditorType::None:
    case EditorType::Text:    mEditorType = "Text"; break;
    case EditorType::Shader:  mEditorType = "Shader"; break;
    case EditorType::Script:  mEditorType = "Script"; break;
    case EditorType::Binary:  mEditorType = "Binary"; break;
    case EditorType::Texture: mEditorType = "Texture"; break;
    case EditorType::QmlView: mEditorType = "QmlView"; break;
    }

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
