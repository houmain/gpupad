#ifndef GPUPADSCRIPTOBJECT_H
#define GPUPADSCRIPTOBJECT_H

#include "MessageList.h"
#include <QJsonValue>
#include <QJsonArray>
#include <QJsonObject>
#include <QJSValue>
#include <functional>

class ScriptEngine;

class GpupadScriptObject final : public QObject
{
    Q_OBJECT
public:
    explicit GpupadScriptObject(QObject *parent = nullptr);

    void initialize(ScriptEngine &engine);
    void applySessionUpdate(ScriptEngine &engine);

    Q_INVOKABLE QJsonArray getItems() const;
    Q_INVOKABLE void updateItems(QJsonValue update);
    Q_INVOKABLE void deleteItem(QJsonValue item);

    Q_INVOKABLE void setBlockData(QJsonValue item, QJSValue data);
    Q_INVOKABLE QString openFileDialog();
    Q_INVOKABLE QString readTextFile(const QString &fileName);
    Q_INVOKABLE bool openWebDock();

private:
    MessagePtrSet mMessages;
    std::vector<std::function<void()>> mPendingUpdates;
    std::vector<std::function<void()>> mPendingEditorUpdates;
    bool mEditorDataUpdated{ };
};

#endif // GPUPADSCRIPTOBJECT_H
