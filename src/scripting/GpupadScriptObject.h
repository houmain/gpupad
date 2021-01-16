#ifndef GPUPADSCRIPTOBJECT_H
#define GPUPADSCRIPTOBJECT_H

#include <QObject>
#include <QJsonValue>
#include <QJsonArray>
#include <QJSValue>

struct Item;
class SessionModel;

class GpupadScriptObject final : public QObject
{
    Q_OBJECT
public:
    explicit GpupadScriptObject(QObject *parent = nullptr);

    Q_INVOKABLE QJsonArray getItems() const;
    Q_INVOKABLE QJsonArray getItemsByName(const QString &name) const;

    Q_INVOKABLE void updateItems(QJsonValue update,
        QJsonValue parent = { }, int row = -1);
    Q_INVOKABLE void deleteItem(QJsonValue item);

    Q_INVOKABLE void setBlockData(QJsonValue item, QJSValue data);

    Q_INVOKABLE QJsonValue openFileDialog();
    Q_INVOKABLE QJsonValue readTextFile(const QString &fileName);

    void finishSessionUpdate();

private:
    SessionModel &sessionModel();
    const SessionModel &sessionModel() const;
    const Item *findItem(QJsonValue objectOrId) const;

    bool mUpdatingSession{ };
};

#endif // GPUPADSCRIPTOBJECT_H
