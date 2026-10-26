#include "MessageList.h"
#include <QMutex>
#include <atomic>
#include <memory>
#include <utility>

namespace {
    QMutex gMessagesMutex;
    QList<std::weak_ptr<const Message>> gMessages;
    MessageId gNextMessageId{ 1 };
    MessagesSnapshot gPublishedSnapshot{ .revision = 1 };
    std::atomic<int> gMessageUpdateDepth{ };

    std::atomic<qulonglong> &messagesRevision()
    {
        // Message shared pointers can outlive other translation-unit statics.
        // Keep the revision counter valid until process termination.
        static auto *revision = new std::atomic<qulonglong>{ 1 };
        return *revision;
    }

    void markMessagesChanged()
    {
        if (gMessageUpdateDepth > 0)
            return;
        ++messagesRevision();
    }

    void refreshPublishedMessagesLocked(qulonglong revision)
    {
        gPublishedSnapshot.clear();

        QMutableListIterator<std::weak_ptr<const Message>> it(gMessages);
        while (it.hasNext()) {
            if (MessagePtr message = it.next().lock()) {
                gPublishedSnapshot += *message;
            } else {
                it.remove();
            }
        }

        gPublishedSnapshot.revision = revision;
    }
} // namespace

MessagePtr MessageList::makeMessage(ItemId itemId, MessageType type,
    QString text, QString fileName, int line)
{
    QMutexLocker lock(&gMessagesMutex);
    const auto messagePtr =
        MessagePtr(new Message{ gNextMessageId++, type, std::move(text), itemId,
                       std::move(fileName), line },
            [](const Message *message) {
                delete message;
                markMessagesChanged();
            });
    gMessages.append(messagePtr);
    markMessagesChanged();
    return messagePtr;
}

void MessageList::insert(MessagePtr message)
{
    if (message)
        QList::append(std::move(message));
}

MessagesSnapshot MessageList::getMessagesSnapshot()
{
    QMutexLocker lock(&gMessagesMutex);
    const auto revision = messagesRevision().load();
    if (gMessageUpdateDepth == 0 && gPublishedSnapshot.revision != revision)
        refreshPublishedMessagesLocked(revision);
    return gPublishedSnapshot;
}

void MessageList::beginUpdate()
{
    ++gMessageUpdateDepth;
}

void MessageList::endUpdate()
{
    QMutexLocker lock(&gMessagesMutex);
    const auto depth = gMessageUpdateDepth.load();
    Q_ASSERT(depth > 0);
    if (depth > 1) {
        --gMessageUpdateDepth;
    } else {
        const auto revision = ++messagesRevision();
        refreshPublishedMessagesLocked(revision);
        gMessageUpdateDepth = 0;
    }
}
