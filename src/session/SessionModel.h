#pragma once

#include "SessionModelCore.h"
#include <QSet>
#include <QFont>
#include <QIcon>
#include <QJsonObject>
#include <QJsonArray>

class SessionModel final : public SessionModelCore
{
    Q_OBJECT
public:
    explicit SessionModel(QObject *parent = nullptr);
    ~SessionModel() override;

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

    QList<QMetaObject::Connection> connectUndoActions(QAction *undo, QAction *redo);
    bool isUndoStackClean();
    void clearUndoStack();
    void beginUndoMacro(const QString &text);
    void endUndoMacro();

    QIcon getTypeIcon(Item::Type type) const;
    QString getItemName(ItemId id) const;
    QString getFullItemName(ItemId id) const;
    void setActiveItems(QSet<ItemId> itemIds);
    void setItemActive(ItemId id, bool active);
    void setActiveItemColor(QColor color);

    QJsonArray getJson(const QModelIndexList &indexes) const;
    void dropJson(const QJsonArray &json,
        int row, const QModelIndex &parent, bool updateExisting);
    bool save(const QString &fileName);
    bool load(const QString &fileName);

    template<typename F> // F(const Item&)
    void forEachItem(const F &function) const
    {
        forEachItemRec(getItem(QModelIndex{ }), false, function);
    }

    template<typename F> // F(const Item&)
    void forEachItemScoped(const QModelIndex &index, const F &function) const
    {
        const auto &pos = getItem(index);
        if (pos.parent)
            forEachItemRecUp(*pos.parent, pos, function);
    }

    template<typename F> // F(const FileItem&)
    void forEachFileItem(const F &function)
    {
        forEachItem([&](const Item &item) {
            if (auto fileItem = castItem<FileItem>(item))
                function(*fileItem);
        });
    }

Q_SIGNALS:
    void undoStackCleanChanged(bool isClean);

private:
    bool shouldSerializeColumn(const Item &item, ColumnType column) const;
    QJsonArray generateJsonFromUrls(QModelIndex target, const QList<QUrl> &urls) const;
    QJsonArray parseDraggedJson(QModelIndex target, const QMimeData *data) const;
    void serialize(QJsonObject &object, const Item &item, bool relativeFilePaths) const;
    void deserialize(const QJsonObject &object, const QModelIndex &parent, int row,
        bool updateExisting);

    template<typename F>
    void forEachItemRec(const Item &item, bool scoped, const F &function) const
    {
        function(item);

        for (const auto *child : item.items) {
            auto visible = true;
            if (scoped && item.type == Item::Type::Group)
                visible = castItem<Group>(item)->inlineScope;
            if (visible)
                forEachItemRec(*child, scoped, function);
        }
    }

    template<typename F> // F(const Item&)
    void forEachItemRecUp(const Item &parent, const Item &pos,
        const F &function) const
    {
        // first traverse up
        if (parent.parent)
            forEachItemRecUp(*parent.parent, parent, function);

        // then recursively traverse siblings until pos is found
        for (const auto *child : parent.items) {
            if (child->id == pos.id)
                break;
            forEachItemRec(*child, true, function);
        }
    }

    QMap<Item::Type, QIcon> mTypeIcons;
    QSet<ItemId> mActiveItemIds;
    QMap<ItemId, ItemId> mDroppedIdsReplaced;
    QModelIndexList mDroppedReferences;
    QColor mActiveItemsColor;
    mutable QModelIndexList mDraggedIndices;
    mutable QString mDraggedText;
    mutable QJsonArray mDraggedJson;
};

