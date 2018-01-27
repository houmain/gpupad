#ifndef GPUPADSCRIPTOBJECT_H
#define GPUPADSCRIPTOBJECT_H

#include <QObject>
#include <QJsonValue>
#include <QJsonArray>

struct Item;
class SessionModel;

class GpupadScriptObject : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QJsonArray session READ session)
public:
    explicit GpupadScriptObject(QObject *parent = nullptr);

    QJsonArray session();
    Q_INVOKABLE QJsonValue openFileDialog();
    Q_INVOKABLE QJsonValue readTextFile(const QString &fileName);
    Q_INVOKABLE void insertItems(QJsonValue update,
        QJsonValue parent = { }, int row = -1);
    Q_INVOKABLE void updateItems(QJsonValue update,
        QJsonValue parent = { }, int row = -1);
    Q_INVOKABLE void deleteItem(QJsonValue item);

    void finishSessionUpdate();

private:
    QModelIndex findItem(QJsonValue value);
    SessionModel &sessionModel();

    bool mUpdatingSession{ };
};

#endif // GPUPADSCRIPTOBJECT_H
