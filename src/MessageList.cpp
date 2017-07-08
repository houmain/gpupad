#include "MessageList.h"

MessagePtr MessageList::insert(QString fileName, int line,
    MessageType type, QString text, bool deduplicate)
{
    QMutexLocker lock(&mMessagesMutex);
    if (deduplicate) {
        foreach (const QWeakPointer<const Message> &ptr, mMessages)
            if (MessagePtr message = ptr.lock())
                if (message->fileName == fileName &&
                    message->line == line &&
                    message->type == type &&
                    message->text == text)
                    return message;
    }
    auto message = MessagePtr(new Message{ type, text, 0, fileName, line });
    mMessages.append(message);
    return message;
}

MessagePtr MessageList::insert(ItemId itemId,
    MessageType type, QString text, bool deduplicate)
{
    QMutexLocker lock(&mMessagesMutex);
    if (deduplicate) {
        foreach (const QWeakPointer<const Message> &ptr, mMessages)
            if (MessagePtr message = ptr.lock())
                if (message->itemId == itemId &&
                    message->type == type &&
                    message->text == text)
                    return message;
    }
    auto message = MessagePtr(new Message{ type, text, itemId, QString(), 0 });
    mMessages.append(message);
    return message;
}

QList<MessagePtr> MessageList::messages() const
{
    QMutexLocker lock(&mMessagesMutex);
    auto result = QList<MessagePtr>();
    QMutableListIterator<QWeakPointer<const Message>> it(mMessages);
    while (it.hasNext()) {
        if (MessagePtr message = it.next().lock())
            result += message;
        else
            it.remove();
    }
    return result;
}
