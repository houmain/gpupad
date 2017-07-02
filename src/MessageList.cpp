#include "MessageList.h"

MessagePtr MessageList::insert(QString fileName, int line, MessageType type, QString text)
{
    QMutexLocker lock(&mMessagesMutex);
    foreach (const QWeakPointer<const Message> &ptr, mMessages)
        if (MessagePtr message = ptr.lock())
            if (message->fileName == fileName &&
                message->line == line &&
                message->type == type &&
                message->text == text)
                return message;
    MessagePtr message(new Message{ type, text, 0, fileName, line });
    mMessages.append(message);

    emit messagesChanged(QPrivateSignal());

    return message;
}

MessagePtr MessageList::insert(ItemId itemId, MessageType type, QString text)
{
    QMutexLocker lock(&mMessagesMutex);
    foreach (const QWeakPointer<const Message> &ptr, mMessages)
        if (MessagePtr message = ptr.lock())
            if (message->itemId == itemId &&
                message->type == type &&
                message->text == text)
                return message;
    MessagePtr message(new Message{ type, text, itemId, QString(), 0 });
    mMessages.append(message);

    emit messagesChanged(QPrivateSignal());

    return message;
}

MessagePtrList MessageList::messages() const
{
    QMutexLocker lock(&mMessagesMutex);
    MessagePtrList result;
    QMutableListIterator<QWeakPointer<const Message>> it(mMessages);
    while (it.hasNext()) {
        if (MessagePtr message = it.next().lock())
            result += message;
        else
            it.remove();
    }
    return result;
}
