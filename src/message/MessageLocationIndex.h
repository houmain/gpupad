#pragma once

#include "Message.h"
#include <QHash>
#include <QList>

class SessionModel;

class MessageLocationIndex
{
public:
    explicit MessageLocationIndex(const SessionModel &sessionModel);

    ItemId resolveItemId(const Message &message) const;
    QString getFullItemName(ItemId itemId) const;
    void sortMessages(QList<Message> &messages) const;

private:
    struct ItemLocation
    {
        quint32 hierarchyRank{ };
        QString fullName;
    };

    struct OrderKey
    {
        quint32 hierarchyRank{ };
        QString fallbackFileName;
        MessageId id{ };
    };

    static QString getFileSortKey(QString fileName);
    const ItemLocation *find(ItemId itemId) const;
    OrderKey getOrderKey(const Message &message) const;

    QHash<ItemId, ItemLocation> mLocations;
    QHash<QString, ItemId> mUniqueItemIdByFileName;
};
