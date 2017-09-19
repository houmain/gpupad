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
    return Singletons::sessionModel().getJson({ QModelIndex() });
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

void GpupadScriptObject::insertSessionItem(QJsonValue parent, int row, QJsonValue item)
{
    auto &model = Singletons::sessionModel();
    auto index = model.index(model.findItem(parent.toInt()));
    if (item.isArray())
        model.dropJson(QJsonDocument(item.toArray()), row, index, false);
    else if (item.isObject())
        model.dropJson(QJsonDocument(item.toObject()), row, index, false);
}

void GpupadScriptObject::updateSession(QJsonValue update)
{
    auto &model = Singletons::sessionModel();
    if (update.isArray())
        model.dropJson(QJsonDocument(update.toArray()), 0, { }, true);
    else if (update.isObject())
        model.dropJson(QJsonDocument(update.toObject()), 0, { }, true);
}
