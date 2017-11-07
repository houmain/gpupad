#include "MessageList.h"
#include <QMutex>

namespace {
    QMutex gMessagesMutex;
    QList<QWeakPointer<const Message>> gMessages;
    qulonglong gNextMessageId;
} // namespace

namespace MessageList {

MessagePtr insert(QString fileName, int line,
    MessageType type, QString text, bool deduplicate)
{
    QMutexLocker lock(&gMessagesMutex);
    if (deduplicate) {
        for (const auto &ptr : qAsConst(gMessages))
            if (MessagePtr message = ptr.lock())
                if (message->fileName == fileName &&
                    message->line == line &&
                    message->type == type &&
                    message->text == text)
                    return message;
    }
    auto message = MessagePtr(new Message{
        gNextMessageId++, type, text, 0, fileName, line });
    gMessages.append(message);
    return message;
}

MessagePtr insert(ItemId itemId,
    MessageType type, QString text, bool deduplicate)
{
    QMutexLocker lock(&gMessagesMutex);
    if (deduplicate) {
        foreach (const QWeakPointer<const Message> &ptr, gMessages)
            if (MessagePtr message = ptr.lock())
                if (message->itemId == itemId &&
                    message->type == type &&
                    message->text == text)
                    return message;
    }
    auto message = MessagePtr(new Message{
        gNextMessageId++, type, text, itemId, QString(), 0 });
    gMessages.append(message);
    return message;
}

QList<MessagePtr> messages()
{
    QMutexLocker lock(&gMessagesMutex);
    auto result = QList<MessagePtr>();
    QMutableListIterator<QWeakPointer<const Message>> it(gMessages);
    while (it.hasNext()) {
        if (MessagePtr message = it.next().lock())
            result += message;
        else
            it.remove();
    }
    return result;
}

} // namespace
