#ifndef SESSIONMODELCORE_H
#define SESSIONMODELCORE_H

#include "Item.h"
#include <QAbstractItemModel>
#include <QUndoStack>

class SessionModelCore : public QAbstractItemModel
{
    Q_OBJECT
public:
    enum ColumnType
    {
        Name = 0,
        None,
        FileName,

        GroupInlineScope,
        GroupIterations,
        BlockOffset,
        BlockRowCount,
        FieldDataType,
        FieldCount,
        FieldPadding,
        TextureTarget,
        TextureFormat,
        TextureWidth,
        TextureHeight,
        TextureDepth,
        TextureLayers,
        TextureSamples,
        TextureFlipVertically,
        ScriptExecuteOn,
        ShaderType,
        ShaderLanguage,
        ShaderEntryPoint,
        BindingType,
        BindingEditor,
        BindingValues,
        BindingTextureId,
        BindingBufferId,
        BindingBlockId,
        BindingLevel,
        BindingLayer,
        BindingImageFormat,
        BindingMinFilter,
        BindingMagFilter,
        BindingAnisotropic,
        BindingWrapModeX,
        BindingWrapModeY,
        BindingWrapModeZ,
        BindingBorderColor,
        BindingComparisonFunc,
        BindingSubroutine,
        AttributeFieldId,
        AttributeNormalize,
        AttributeDivisor,
        AttachmentTextureId,
        AttachmentLevel,
        AttachmentLayer,
        AttachmentBlendColorEq,
        AttachmentBlendColorSource,
        AttachmentBlendColorDest,
        AttachmentBlendAlphaEq,
        AttachmentBlendAlphaSource,
        AttachmentBlendAlphaDest,
        AttachmentColorWriteMask,
        AttachmentDepthComparisonFunc,
        AttachmentDepthOffsetFactor,
        AttachmentDepthOffsetUnits,
        AttachmentDepthClamp,
        AttachmentDepthWrite,
        AttachmentStencilFrontComparisonFunc,
        AttachmentStencilFrontReference,
        AttachmentStencilFrontReadMask,
        AttachmentStencilFrontFailOp,
        AttachmentStencilFrontDepthFailOp,
        AttachmentStencilFrontDepthPassOp,
        AttachmentStencilFrontWriteMask,
        AttachmentStencilBackComparisonFunc,
        AttachmentStencilBackReference,
        AttachmentStencilBackReadMask,
        AttachmentStencilBackFailOp,
        AttachmentStencilBackDepthFailOp,
        AttachmentStencilBackDepthPassOp,
        AttachmentStencilBackWriteMask,
        TargetFrontFace,
        TargetCullMode,
        TargetPolygonMode,
        TargetLogicOperation,
        TargetBlendConstant,
        TargetDefaultWidth,
        TargetDefaultHeight,
        TargetDefaultLayers,
        TargetDefaultSamples,
        CallChecked,
        CallType,
        CallProgramId,
        CallTargetId,
        CallVertexStreamId,
        CallPrimitiveType,
        CallPatchVertices,
        CallCount,
        CallFirst,
        CallIndexBufferBlockId,
        CallBaseVertex,
        CallInstanceCount,
        CallBaseInstance,
        CallIndirectBufferBlockId,
        CallDrawCount,
        CallWorkGroupsX,
        CallWorkGroupsY,
        CallWorkGroupsZ,
        CallBufferId,
        CallFromBufferId,
        CallTextureId,
        CallFromTextureId,
        CallClearColor,
        CallClearDepth,
        CallClearStencil,
        CallExecuteOn,
    };

    explicit SessionModelCore(QObject *parent = nullptr);
    SessionModelCore(const SessionModelCore& rhs);
    SessionModelCore &operator=(const SessionModelCore &rhs);
    ~SessionModelCore() override;

    void clear();
    QModelIndex index(int row, int column,
          const QModelIndex &parent = QModelIndex()) const override;
    QModelIndex parent(const QModelIndex &child) const override;

    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    int columnCount(const QModelIndex &parent = QModelIndex()) const override;

    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;
    bool setData(const QModelIndex &index, const QVariant &value,
        int role = Qt::EditRole) override;
    bool removeRows(int row, int count, const QModelIndex &parent) override;

    Item::Type getTypeByName(const QString &name, bool &ok) const;
    QString getTypeName(Item::Type type) const;
    bool canContainType(const QModelIndex &index, Item::Type type) const;
    QModelIndex insertItem(Item::Type type, QModelIndex parent,
        int row = -1, ItemId id = 0);
    void deleteItem(const QModelIndex &index);
    QModelIndex getIndex(const Item *item, ColumnType column = ColumnType::None) const;
    QModelIndex getIndex(const QModelIndex &index, ColumnType column) const;
    const Item* findItem(ItemId id) const;
    const Item &getItem(const QModelIndex &index) const;
    ItemId getItemId(const QModelIndex &index) const;
    Item::Type getItemType(const QModelIndex &index) const;

    template<typename T>
    const T *item(const QModelIndex &index) const
    {
        return castItem<T>(&getItem(index));
    }

    template<typename T>
    const T *findItem(ItemId id) const
    {
        return castItem<T>(SessionModelCore::findItem(id));
    }

protected:
    const Group &root() const { return *mRoot; }
    ItemId getNextItemId();
    QUndoStack &undoStack() { return mUndoStack; }

private:
    Item &getItemRef(const QModelIndex &index);
    void insertItem(Item *item, const QModelIndex &parent, int row = -1);
    void insertItem(QList<Item*> *list, Item *item,
        const QModelIndex &parent, int row);
    void removeItem(QList<Item*> *list,
        const QModelIndex &parent, int row);
    void pushUndoCommand(QUndoCommand *command);
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
    bool hasChildWithName(const QModelIndex &parent, const QString &name);

    ItemId mNextItemId{ 1 };
    QScopedPointer<Group> mRoot;
    QUndoStack mUndoStack;
    QMap<ItemId, const Item*> mItemsById;
};

#endif // SESSIONMODELCORE_H
