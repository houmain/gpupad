#include "EditorScriptObject.h"
#include <QJsonArray>

EditorScriptObject::EditorScriptObject(const QString &fileName, QObject *parent)
    : QObject(parent)
    , mFileName(fileName)
{
}

QJsonValue EditorScriptObject::viewportSize() const
{
    return QJsonArray({ 100, 100 });
}
