#include "MessageLocationIndex.h"
#include "session/SessionModel.h"
#include <QDir>
#include <QSet>
#include <algorithm>
#include <limits>
#include <tuple>
#include <utility>

MessageLocationIndex::MessageLocationIndex(const SessionModel &sessionModel)
{
    auto ambiguousFileNames = QSet<QString>{ };
    auto hierarchyRank = quint32{ };
    sessionModel.forEachItem([&](const Item &item) {
        if (!item.id)
            return;

        mLocations.insert(item.id,
            ItemLocation{
                .hierarchyRank = hierarchyRank++,
                .fullName = sessionModel.getFullItemName(item.id),
            });
        if (const auto fileItem = castItem<FileItem>(item)) {
            const auto key = getFileSortKey(fileItem->fileName);
            if (key.isEmpty())
                return;
            if (mUniqueItemIdByFileName.contains(key)) {
                mUniqueItemIdByFileName.remove(key);
                ambiguousFileNames.insert(key);
            } else if (!ambiguousFileNames.contains(key)) {
                mUniqueItemIdByFileName.insert(key, item.id);
            }
        }
    });
}

ItemId MessageLocationIndex::resolveItemId(const Message &message) const
{
    if (mLocations.contains(message.itemId))
        return message.itemId;
    return mUniqueItemIdByFileName.value(getFileSortKey(message.fileName));
}

QString MessageLocationIndex::getFullItemName(ItemId itemId) const
{
    const auto location = find(itemId);
    return (location ? location->fullName : QString{ });
}

void MessageLocationIndex::sortMessages(QList<Message> &messages) const
{
    auto keys = QHash<MessageId, OrderKey>{ };
    keys.reserve(messages.size());
    for (const auto &message : std::as_const(messages))
        keys.insert(message.id, getOrderKey(message));
    std::sort(messages.begin(), messages.end(),
        [&](const Message &a, const Message &b) {
            const auto &aKey = *keys.constFind(a.id);
            const auto &bKey = *keys.constFind(b.id);
            return std::tie(aKey.hierarchyRank, aKey.fallbackFileName, aKey.id)
                < std::tie(bKey.hierarchyRank, bKey.fallbackFileName, bKey.id);
        });
}

QString MessageLocationIndex::getFileSortKey(QString fileName)
{
    if (fileName.isEmpty())
        return { };
    fileName = QDir::cleanPath(fileName);
#if defined(Q_OS_WIN)
    fileName = fileName.toCaseFolded();
#endif
    return fileName;
}

const MessageLocationIndex::ItemLocation *MessageLocationIndex::find(
    ItemId itemId) const
{
    const auto it = mLocations.constFind(itemId);
    return (it != mLocations.cend() ? &*it : nullptr);
}

MessageLocationIndex::OrderKey MessageLocationIndex::getOrderKey(
    const Message &message) const
{
    const auto itemId = resolveItemId(message);
    const auto location = find(itemId);
    return {
        .hierarchyRank = (location ? location->hierarchyRank
                                   : std::numeric_limits<quint32>::max()),
        // Preserve production order within a session item. Messages without a
        // resolvable item use their filename as a stable grouping key.
        .fallbackFileName =
            (location ? QString{ } : getFileSortKey(message.fileName)),
        .id = message.id,
    };
}
