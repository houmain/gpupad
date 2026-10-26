#pragma once

#include "Message.h"
#include <QList>

class MessagePtrSet : QList<MessagePtr>
{
public:
    static MessagePtr makeMessage(ItemId itemId, MessageType type, QString text,
        QString fileName, int line);
    static QList<MessagePtr> getAllMessages();

    using QList::clear;
    using QList::size;

    void insert(MessagePtr message);

    MessagePtrSet &operator+=(const MessagePtr &message)
    {
        insert(message);
        return *this;
    }

    MessagePtrSet &operator+=(const MessagePtrSet &other)
    {
        if (this == &other)
            return *this;
        for (const auto &message : other)
            insert(message);
        return *this;
    }

    void insert(QString fileName, int line, MessageType type, QString text = "")
    {
        insert(makeMessage(0, type, text, fileName, line));
    }

    void insert(ItemId itemId, MessageType type, QString text = "")
    {
        insert(makeMessage(itemId, type, text, "", 0));
    }

    void insert(ItemId itemId, MessageType type, QString text, QString fileName,
        int line)
    {
        insert(makeMessage(itemId, type, text, fileName, line));
    }

    void insert(MessageType type, QString text = "")
    {
        insert(makeMessage(0, type, text, "", 0));
    }
};
