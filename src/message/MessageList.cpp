#include "MessageList.h"
#include <QMutex>
#include <memory>
#include <utility>

namespace {
    QMutex gMessagesMutex;
    QList<std::weak_ptr<const Message>> gMessages;
    MessageId gNextMessageId{ 1 };
} // namespace

MessagePtr MessageList::makeMessage(ItemId itemId, MessageType type,
    QString text, QString fileName, int line)
{
    QMutexLocker lock(&gMessagesMutex);
    const auto message = std::make_shared<const Message>(Message{
        gNextMessageId++, type, std::move(text), itemId,
        std::move(fileName), line });
    gMessages.append(message);
    return message;
}

QList<Message> MessageList::getAllMessages()
{
    QMutexLocker lock(&gMessagesMutex);
    auto result = QList<Message>{};
    QMutableListIterator<std::weak_ptr<const Message>> it(gMessages);
    while (it.hasNext()) {
        if (auto message = it.next().lock())
            result.append(*message);
        else
            it.remove();
    }
    return result;
}

void MessageList::insert(MessagePtr message)
{
    if (message)
        QList::append(std::move(message));
}
