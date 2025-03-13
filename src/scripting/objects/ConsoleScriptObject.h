#pragma once

#include "MessageList.h"
#include <QObject>

class ConsoleScriptObject : public QObject
{
    Q_OBJECT
public:
    explicit ConsoleScriptObject(QObject *parent = 0);

    void setMessages(MessagePtrSet *messages, const QString &fileName);
    void setMessages(MessagePtrSet *messages, ItemId itemId);

    Q_INVOKABLE void output(QString message, int level);

private:
    QString mFileName;
    ItemId mItemId{};
    MessagePtrSet *mMessages{};
};
