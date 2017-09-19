#ifndef SESSIONMODEL_H
#define SESSIONMODEL_H

#include "SessionModelCore.h"
#include <QSet>
#include <QFont>
#include <QJsonObject>
#include <QJsonArray>

class SessionModel : public SessionModelCore
{
    Q_OBJECT
public:
    explicit SessionModel(QObject *parent = 0);
    ~SessionModel();

    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;
    Qt::ItemFlags flags(const QModelIndex &index) const override;

    Qt::DropActions supportedDragActions() const override;
    Qt::DropActions supportedDropActions() const override;
    QStringList mimeTypes() const override;
    QMimeData *mimeData(const QModelIndexList &indexes) const override;
    bool canDropMimeData(const QMimeData *data, Qt::DropAction action,
        int row, int column, const QModelIndex &parent) const override;
    bool dropMimeData(const QMimeData *data, Qt::DropAction action,
        int row, int column, const QModelIndex &parent) override;
    bool removeRows(int row, int count,
        const QModelIndex &parent = QModelIndex()) override;

    QIcon getTypeIcon(Item::Type type) const;
    QString findItemName(ItemId id) const;
    void setActiveItems(QSet<ItemId> itemIds);
    void setItemActive(ItemId id, bool active);

    QJsonArray getJson(const QModelIndexList &indexes) const;
    void dropJson(const QJsonDocument &document,
        int row, const QModelIndex &parent, bool updateExisting);
    void clear();
    bool save(const QString &fileName);
    bool load(const QString &fileName);

    template<typename T>
    const T *item(const QModelIndex &index) const
    {
        return castItem<T>(&getItem(index));
    }

    using SessionModelCore::findItem;

    template<typename T>
    const T *findItem(ItemId id) const
    {
        return castItem<T>(SessionModelCore::findItem(id));
    }

    template<typename F> // F(const Item&)
    void forEachItem(const F &function) const
    {
        forEachItemRec(getItem(QModelIndex{ }), false, function);
    }

    template<typename F> // F(const Item&)
    void forEachItemScoped(const QModelIndex &index, const F &function) const
    {
        const auto& pos = getItem(index);
        if (pos.parent)
            forEachItemRecUp(*pos.parent, pos, function);
    }

private:
    const QJsonDocument *parseClipboard(const QMimeData *data) const;
    void serialize(QJsonObject &object, const Item &item, bool relativeFilePaths) const;
    void deserialize(const QJsonObject &object, const QModelIndex &parent, int row,
        bool updateExisting);

    template<typename F>
    void forEachItemRec(const Item &item, bool scoped, const F &function) const
    {
        function(item);

        foreach (const Item *child, item.items)
            if (!scoped || item.inlineScope)
                forEachItemRec(*child, scoped, function);
    }

    template<typename F> // F(const Item&)
    void forEachItemRecUp(const Item &parent, const Item &pos,
        const F &function) const
    {
        // first traverse up
        if (parent.parent)
            forEachItemRecUp(*parent.parent, parent, function);

        // then recursively traverse siblings until pos is found
        foreach (const Item *child, parent.items) {
            if (child->id == pos.id)
                break;
            forEachItemRec(*child, true, function);
        }
    }

    QMap<Item::Type, QIcon> mTypeIcons;
    QSet<ItemId> mActiveItemIds;
    QColor mActiveColor;
    QFont mActiveCallFont;
    QMap<ItemId, ItemId> mDroppedIdsReplaced;
    QList<ItemId*> mDroppedReferences;
    mutable QList<QModelIndex> mDraggedIndices;
    mutable const QMimeData *mClipboardData{ };
    mutable QScopedPointer<QJsonDocument> mClipboardJsonDocument;
};

#endif // SESSIONMODEL_H
