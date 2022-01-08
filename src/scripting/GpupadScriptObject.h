#ifndef GPUPADSCRIPTOBJECT_H
#define GPUPADSCRIPTOBJECT_H

#include "MessageList.h"

class GpupadScriptObject final : public QObject
{
    Q_OBJECT
public:
    explicit GpupadScriptObject(QObject *parent = nullptr);

    Q_INVOKABLE QString openFileDialog();
    Q_INVOKABLE QString readTextFile(const QString &fileName);
    Q_INVOKABLE bool openQmlDock();
    Q_INVOKABLE bool openWebDock();

private:
    MessagePtrSet mMessages;
};

#endif // GPUPADSCRIPTOBJECT_H
