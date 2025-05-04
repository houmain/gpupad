#pragma once

#include "MessageList.h"
#include <QObject>

class ConsoleScriptObject : public QObject
{
    Q_OBJECT
public:
    explicit ConsoleScriptObject(MessagePtrSet *messages, QObject *parent = 0);

    void setFileName(const QString &fileName);
    void setItemId(ItemId itemId);

    Q_INVOKABLE void output(QString message, int level);

private:
    MessagePtrSet &mMessages;
    QString mFileName;
    ItemId mItemId{};
};
