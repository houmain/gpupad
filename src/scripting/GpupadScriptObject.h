#ifndef GPUPADSCRIPTOBJECT_H
#define GPUPADSCRIPTOBJECT_H

#include <QObject>
#include <QJsonValue>
#include <QJsonArray>

class GpupadScriptObject : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QJsonArray session READ session)
public:
    explicit GpupadScriptObject(QObject *parent = nullptr);

    QJsonArray session();
    Q_INVOKABLE QJsonValue openFileDialog();
    Q_INVOKABLE QJsonValue readTextFile(const QString &fileName);
    Q_INVOKABLE void insertSessionItem(QJsonValue parent, int row, QJsonValue item);
    Q_INVOKABLE void updateSession(QJsonValue update);
};

#endif // GPUPADSCRIPTOBJECT_H
