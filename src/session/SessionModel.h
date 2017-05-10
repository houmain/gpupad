#ifndef SESSIONMODEL_H
#define SESSIONMODEL_H

#include "Item.h"
#include <QAbstractItemModel>
#include <QUndoStack>
#include <QSet>
#include <QFont>

class QXmlStreamWriter;
class QXmlStreamReader;

class SessionModel : public QAbstractItemModel
{
    Q_OBJECT
public:
    enum ColumnType
    {
        Name = 0,
        InlineScope,
        FileName,
        BufferOffset,
        BufferRowCount,
        ColumnDataType,
        ColumnCount,
        ColumnPadding,
        TextureTarget,
        TextureFormat,
        TextureWidth,
        TextureHeight,
        TextureDepth,
        ImageLevel,
        ImageLayer,
        ImageFace,
        SamplerTextureId,
        SamplerMinFilter,
        SamplerMagFilter,
        SamplerWarpModeX,
        SamplerWarpModeY,
        SamplerWarpModeZ,
        ShaderType,
        BindingType,
        BindingValues,
        PrimitivesType,
        PrimitivesIndexBufferId,
        PrimitivesFirstVertex,
        PrimitivesVertexCount,
        PrimitivesInstanceCount,
        PrimitivesPatchVertices,
        AttributeBufferId,
        AttributeColumnId,
        AttributeNormalize,
        AttributeDivisor,
        AttachmentTextureId,
        CallType,
        CallProgramId,
        CallPrimitivesId,
        CallFramebufferId,
        CallNumGroupsX,
        CallNumGroupsY,
        CallNumGroupsZ,
        StateType,
    };

    explicit SessionModel(QObject *parent = 0);
    ~SessionModel();

    QModelIndex index(int row, int column,
          const QModelIndex &parent = QModelIndex()) const override;
    QModelIndex parent(const QModelIndex &child) const override;

    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    int columnCount(const QModelIndex &parent = QModelIndex()) const override;

    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;
    bool setData(const QModelIndex &index, const QVariant &value,
        int role = Qt::EditRole) override;
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

    QIcon getTypeIcon(ItemType type) const;
    QString getTypeName(ItemType type) const;
    bool canContainType(const QModelIndex &index, ItemType type) const;

    QModelIndex insertItem(ItemType type,
        const QModelIndex &parent, int row = -1, ItemId id = 0);
    void deleteItem(const QModelIndex &index);
    QModelIndex index(const Item *item, int column = 0) const;
    ItemId getItemId(const QModelIndex &index) const;
    ItemType getItemType(const QModelIndex &index) const;
    const Item* findItem(ItemId id) const;
    QString findItemName(ItemId id) const;

    void setActiveItems(QSet<ItemId> itemIds);
    void setItemActive(ItemId id, bool active);

    QUndoStack &undoStack() { return mUndoStack; }
    void clear();
    bool save(const QString &fileName);
    bool load(const QString &fileName);

    template<typename T>
    const T *item(const QModelIndex &index) const {
        return castItem<T>(&getItem(index));
    }

    template<typename T>
    const T *findItem(ItemId id) const {
        return castItem<T>(findItem(id));
    }

    template<typename F> // F(const Item&)
    void forEachItem(const F &function) const {
        forEachItemRec(*mRoot, false, function);
    }

    template<typename F> // F(const Item&)
    void forEachItemScoped(const QModelIndex &index, const F &function) const
    {
        const auto& pos = getItem(index);
        if (pos.parent)
            forEachItemRecUp(*pos.parent, pos, function);
    }

private:
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

    const Item &getItem(const QModelIndex &index) const;
    Item &getItem(const QModelIndex &index);
    void insertItem(Item *item, const QModelIndex &parent, int row = -1);
    void insertItem(QList<Item*> *list, Item *item,
        const QModelIndex &parent, int row);
    void removeItem(QList<Item*> *list,
        const QModelIndex &parent, int row);
    void undoableInsertItem(QList<Item*> *list, Item *item,
        const QModelIndex &parent, int row);
    void undoableRemoveItem(QList<Item*> *list, Item *item,
        const QModelIndex &index);
    template<typename T, typename S>
    void assignment(const QModelIndex &index, T *to, S &&value);
    template<typename T, typename S>
    void undoableAssignment(const QModelIndex &index, T *to, S &&value,
        int mergeId = -1);
    void undoableFileNameAssignment(const QModelIndex &index, FileItem &item,
        QString fileName);
    void serialize(QXmlStreamWriter &xml, const Item &item) const;
    void deserialize(QXmlStreamReader &xml, const QModelIndex &parent, int row);

    ItemId mNextItemId{ 1 };
    Group *mRoot;
    QUndoStack mUndoStack;
    QMap<ItemType, QIcon> mTypeIcons;
    mutable QList<QModelIndex> mDraggedIndices;
    QSet<ItemId> mActiveItemIds;
    QColor mActiveColor;
    QFont mActiveCallFont;
};

#endif // SESSIONMODEL_H
