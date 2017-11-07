#include "GpupadScriptObject.h"
#include "Singletons.h"
#include "FileDialog.h"
#include "FileCache.h"
#include "session/SessionModel.h"
#include <QJsonDocument>

GpupadScriptObject::GpupadScriptObject(QObject *parent) : QObject(parent)
{
}

QJsonArray GpupadScriptObject::session()
{
    return sessionModel().getJson({ QModelIndex() });
}

QJsonValue GpupadScriptObject::openFileDialog()
{
    auto options = FileDialog::Options();
    if (Singletons::fileDialog().exec(options))
        return Singletons::fileDialog().fileName();
    return { };
}

QJsonValue GpupadScriptObject::readTextFile(const QString &fileName)
{
    auto source = QString();
    if (Singletons::fileCache().getSource(fileName, &source))
        return source;
    return { };
}

void GpupadScriptObject::updateSession(QJsonValue update, QJsonValue parent, int row)
{
    auto index = findItem(parent);
    if (!update.isArray())
        update = QJsonArray({ update });
    sessionModel().dropJson(update.toArray(), row, index, true);
}

void GpupadScriptObject::deleteItem(QJsonValue item)
{
    sessionModel().deleteItem(findItem(item));
}

QModelIndex GpupadScriptObject::findItem(QJsonValue value)
{
    return sessionModel().getIndex(
        sessionModel().findItem(value.isObject() ?
            value.toObject()["id"].toInt() :
            value.toInt()));
}

SessionModel &GpupadScriptObject::sessionModel()
{
    if (!std::exchange(mUpdatingSession, true))
        Singletons::sessionModel().undoStack().beginMacro("script");

    return Singletons::sessionModel();
}

void GpupadScriptObject::finishSessionUpdate()
{
    if (std::exchange(mUpdatingSession, false))
        Singletons::sessionModel().undoStack().endMacro();
}
