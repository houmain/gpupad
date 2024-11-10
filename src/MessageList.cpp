#include "MessageList.h"
#include <QMutex>

namespace {
    QMutex gMessagesMutex;
    QList<QWeakPointer<const Message>> gMessages;
    qulonglong gNextMessageId = 1;
} // namespace

namespace MessageList {

    MessagePtr insert(QString fileName, int line, MessageType type,
        QString text, bool deduplicate)
    {
        QMutexLocker lock(&gMessagesMutex);
        if (deduplicate) {
            for (const auto &ptr : std::as_const(gMessages))
                if (MessagePtr message = ptr.lock())
                    if (message->fileName == fileName && message->line == line
                        && message->type == type && message->text == text)
                        return message;
        }
        auto message = MessagePtr(
            new Message{ gNextMessageId++, type, text, 0, fileName, line });
        gMessages.append(message);
        return message;
    }

    MessagePtr insert(ItemId itemId, MessageType type, QString text,
        bool deduplicate)
    {
        QMutexLocker lock(&gMessagesMutex);
        if (deduplicate) {
            for (const QWeakPointer<const Message> &ptr : std::as_const(gMessages))
                if (MessagePtr message = ptr.lock())
                    if ((!itemId || message->itemId == itemId)
                        && message->type == type && message->text == text)
                        return message;
        }
        auto message = MessagePtr(
            new Message{ gNextMessageId++, type, text, itemId, QString(), 0 });
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

} // namespace MessageList

QString formatDuration(const std::chrono::duration<double> &duration)
{
    auto toString = [](double v) { return QString::number(v, 'f', 2); };

    auto seconds = duration.count();
    if (duration > std::chrono::seconds(1))
        return toString(seconds) + "s";

    if (duration > std::chrono::milliseconds(1))
        return toString(seconds * 1000) + "ms";

    if (duration > std::chrono::microseconds(1))
        return toString(seconds * 1000000ull) + QChar(181) + "s";

    return toString(seconds * 1000000000ull) + "ns";
}
