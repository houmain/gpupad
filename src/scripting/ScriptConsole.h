#pragma once

#include "MessageList.h"
#include <QObject>

class ScriptConsole : public QObject
{
    Q_OBJECT
public:
    explicit ScriptConsole(QObject *parent = 0);

    void setMessages(MessagePtrSet *messages, const QString &fileName);
    void setMessages(MessagePtrSet *messages, ItemId itemId);

public Q_SLOTS:
    void output(QString message, int level);

private:
    QString mFileName;
    ItemId mItemId{};
    MessagePtrSet *mMessages{};
};
