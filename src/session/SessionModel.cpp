#include "SessionModel.h"
#include "FileDialog.h"
#include <QIcon>
#include <QMimeData>
#include <QXmlStreamWriter>
#include <QXmlStreamReader>
#include <QDir>

namespace {
    const auto MimeType = QStringLiteral("application/xml");
    const auto MultipleTag = QStringLiteral("multiple");
    const auto GroupTag = QStringLiteral("group");
    const auto BufferTag = QStringLiteral("buffer");
    const auto ColumnTag = QStringLiteral("column");
    const auto TextureTag = QStringLiteral("texture");
    const auto ImageTag = QStringLiteral("image");
    const auto ProgramTag = QStringLiteral("program");
    const auto ShaderTag = QStringLiteral("shader");
    const auto BindingTag = QStringLiteral("binding");
    const auto ValueTag = QStringLiteral("value");
    const auto FieldTag = QStringLiteral("field");
    const auto VertexStreamTag = QStringLiteral("vertexstream");
    const auto AttributeTag = QStringLiteral("attribute");
    const auto TargetTag = QStringLiteral("target");
    const auto AttachmentTag = QStringLiteral("attachment");
    const auto CallTag = QStringLiteral("call");
    const auto ScriptTag = QStringLiteral("script");

    const QString &tagNameByType(ItemType type)
    {
        switch (type) {
            case ItemType::Group: return GroupTag;
            case ItemType::Buffer: return BufferTag;
            case ItemType::Column: return ColumnTag;
            case ItemType::Texture: return TextureTag;
            case ItemType::Image: return ImageTag;
            case ItemType::Program: return ProgramTag;
            case ItemType::Shader: return ShaderTag;
            case ItemType::Binding: return BindingTag;
            case ItemType::VertexStream: return VertexStreamTag;
            case ItemType::Attribute: return AttributeTag;
            case ItemType::Target: return TargetTag;
            case ItemType::Attachment: return AttachmentTag;
            case ItemType::Call: return CallTag;
            case ItemType::Script: return ScriptTag;
        }
        static const QString sEmpty;
        return sEmpty;
    }

    ItemType typeByTagName(QStringRef tag)
    {
        if (tag == GroupTag) return ItemType::Group;
        if (tag == BufferTag) return ItemType::Buffer;
        if (tag == ColumnTag) return ItemType::Column;
        if (tag == TextureTag) return ItemType::Texture;
        if (tag == ImageTag) return ItemType::Image;
        if (tag == "sampler") return ItemType::Binding; // TODO: remove
        if (tag == ProgramTag) return ItemType::Program;
        if (tag == ShaderTag) return ItemType::Shader;
        if (tag == BindingTag) return ItemType::Binding;
        if (tag == VertexStreamTag) return ItemType::VertexStream;
        if (tag == AttributeTag) return ItemType::Attribute;
        if (tag == "framebuffer") return ItemType::Target; // TODO: remove
        if (tag == TargetTag) return ItemType::Target;
        if (tag == AttachmentTag) return ItemType::Attachment;
        if (tag == CallTag) return ItemType::Call;
        if (tag == ScriptTag) return ItemType::Script;
        return { };
    }

    template<typename Redo, typename Undo, typename Free>
    class UndoCommand : public QUndoCommand
    {
    public:
        UndoCommand(QString text,
            Redo &&redo, Undo &&undo, Free &&free, bool owns)
            : QUndoCommand(text)
            , mRedo(std::forward<Redo>(redo))
            , mUndo(std::forward<Undo>(undo))
            , mFree(std::forward<Free>(free))
            , mOwns(owns) { }
        ~UndoCommand() { if (mOwns) mFree(); }
        void redo() override { mRedo(); mOwns = !mOwns; }
        void undo() override { mUndo(); mOwns = !mOwns; }

    private:
        const Redo mRedo;
        const Undo mUndo;
        const Free mFree;
        bool mOwns;
    };

    template<typename Redo, typename Undo, typename Free>
    auto makeUndoCommand(QString text, Redo &&redo, Undo &&undo, Free &&free,
        bool owns = false)
    {
        return new UndoCommand<Redo, Undo, Free>(text,
            std::forward<Redo>(redo),
            std::forward<Undo>(undo),
            std::forward<Free>(free),
            owns);
    }

    class MergingUndoCommand : public QUndoCommand
    {
    public:
        MergingUndoCommand(int id, QUndoCommand *firstCommand)
            : QUndoCommand(firstCommand->text())
            , mId(id)
            , mCommands{ firstCommand } { }
        ~MergingUndoCommand() {
            qDeleteAll(mCommands);
        }
        void redo() override { foreach (QUndoCommand *c, mCommands) c->redo(); }
        void undo() override {
            std::for_each(mCommands.rbegin(), mCommands.rend(),
                [](auto c) { c->undo(); });
        }
        int id() const override { return mId; }
        bool mergeWith(const QUndoCommand* other) override {
            Q_ASSERT(other->id() == id());
            auto &c = static_cast<const MergingUndoCommand*>(other)->mCommands;
            mCommands.append(c);
            c.clear();
            return true;
        }

    private:
        const int mId;
        mutable QList<QUndoCommand*> mCommands;
    };
} // namespace

SessionModel::SessionModel(QObject *parent)
    : QAbstractItemModel(parent)
    , mRoot(new Group())
{
    mTypeIcons[ItemType::Group].addFile(QStringLiteral(":/images/16x16/folder.png"));
    mTypeIcons[ItemType::Buffer].addFile(QStringLiteral(":/images/16x16/x-office-spreadsheet.png"));
    mTypeIcons[ItemType::Column].addFile(QStringLiteral(":/images/16x16/mail-attachment.png"));
    mTypeIcons[ItemType::Texture].addFile(QStringLiteral(":/images/16x16/image-x-generic.png"));
    mTypeIcons[ItemType::Image].addFile(QStringLiteral(":/images/16x16/mail-attachment.png"));
    mTypeIcons[ItemType::Program].addFile(QStringLiteral(":/images/16x16/applications-system.png"));
    mTypeIcons[ItemType::Shader].addFile(QStringLiteral(":/images/16x16/font.png"));
    mTypeIcons[ItemType::Binding].addFile(QStringLiteral(":/images/16x16/insert-text.png"));
    mTypeIcons[ItemType::VertexStream].addFile(QStringLiteral(":/images/16x16/media-playback-start-rtl.png"));
    mTypeIcons[ItemType::Attribute].addFile(QStringLiteral(":/images/16x16/mail-attachment.png"));
    mTypeIcons[ItemType::Target].addFile(QStringLiteral(":/images/16x16/emblem-photos.png"));
    mTypeIcons[ItemType::Attachment].addFile(QStringLiteral(":/images/16x16/mail-attachment.png"));
    mTypeIcons[ItemType::Call].addFile(QStringLiteral(":/images/16x16/dialog-information.png"));
    mTypeIcons[ItemType::Script].addFile(QStringLiteral(":/images/16x16/font.png"));

    mActiveColor = QColor::fromRgb(0, 32, 255);
    mActiveCallFont = QFont();
    mActiveCallFont.setBold(true);
}

SessionModel::~SessionModel()
{
    clear();
}

QIcon SessionModel::getTypeIcon(ItemType type) const
{
    return mTypeIcons[type];
}

QString SessionModel::getTypeName(ItemType type) const
{
    switch (type) {
        case ItemType::Group: return tr("Group");
        case ItemType::Buffer: return tr("Buffer");
        case ItemType::Column: return tr("Column");
        case ItemType::Texture: return tr("Texture");
        case ItemType::Image: return tr("Image");
        case ItemType::Program: return tr("Program");
        case ItemType::Shader: return tr("Shader");
        case ItemType::Binding: return tr("Binding");
        case ItemType::VertexStream: return tr("Vertex Stream");
        case ItemType::Attribute: return tr("Attribute");
        case ItemType::Target: return tr("Target");
        case ItemType::Attachment: return tr("Attachment");
        case ItemType::Call: return tr("Call");
        case ItemType::Script: return tr("Script");
    }
    return "";
}

bool SessionModel::canContainType(const QModelIndex &index, ItemType type) const
{
    auto inList = [](ItemType type, std::initializer_list<ItemType> types) {
        return std::find(cbegin(types), cend(types), type) != cend(types);
    };

    switch (getItemType(index)) {
        case ItemType::Group:
            return inList(type, {
                ItemType::Group,
                ItemType::Buffer,
                ItemType::Texture,
                ItemType::Program,
                ItemType::Binding,
                ItemType::VertexStream,
                ItemType::Target,
                ItemType::Call,
                ItemType::Script,
            });

        case ItemType::Buffer:
            return inList(type, { ItemType::Column });

        case ItemType::Texture:
            return inList(type, { ItemType::Image });

        case ItemType::Program:
            return inList(type, { ItemType::Shader });

        case ItemType::VertexStream:
            return inList(type, { ItemType::Attribute });

        case ItemType::Target:
            return inList(type, { ItemType::Attachment });

        default:
            return false;
    }
}

QModelIndex SessionModel::index(int row, int column,
    const QModelIndex &parent) const
{
    const auto &parentItem = getItem(parent);
    if (row >= 0 && row < parentItem.items.size())
        return index(parentItem.items.at(row), column);

    return { };
}

QModelIndex SessionModel::parent(const QModelIndex &child) const
{
    if (child.isValid())
        return index(getItem(child).parent, 0);
    return { };
}

int SessionModel::rowCount(const QModelIndex &parent) const
{
    if (parent.isValid())
        return getItem(parent).items.size();
    return mRoot->items.size();
}

int SessionModel::columnCount(const QModelIndex &parent) const
{
    Q_UNUSED(parent);
    return 1;
}

QVariant SessionModel::data(const QModelIndex &index, int role) const
{
    if (role == Qt::DecorationRole) {
        auto type = getItemType(index);
        return (type != ItemType::Call ? getTypeIcon(type) : QVariant());
    }

    if (role != Qt::DisplayRole &&
        role != Qt::EditRole &&
        role != Qt::FontRole &&
        role != Qt::ForegroundRole &&
        role != Qt::CheckStateRole)
        return { };

    const auto &item = getItem(index);

    if (role == Qt::FontRole)
        return (item.itemType == ItemType::Call &&
                mActiveItemIds.contains(item.id) ?
             mActiveCallFont : QVariant());

    if (role == Qt::ForegroundRole)
        return (mActiveItemIds.contains(item.id) ?
            mActiveColor : QVariant());

    if (role == Qt::CheckStateRole) {
        auto checked = false;
        switch (item.itemType) {
            case ItemType::Call:
                checked = static_cast<const Call&>(item).checked;
                break;
            default:
                return { };
        }
        return (checked ? Qt::Checked : Qt::Unchecked);
    }

    switch (static_cast<ColumnType>(index.column())) {
        case ColumnType::Name: return item.name;
        case ColumnType::InlineScope: return item.inlineScope;

        case ColumnType::FileName:
            if (auto fileItem = castItem<FileItem>(item))
                return fileItem->fileName;
            break;

#define ADD(COLUMN_TYPE, ITEM_TYPE, PROPERTY) \
        case ColumnType::COLUMN_TYPE: \
            if (item.itemType == ItemType::ITEM_TYPE) \
                return static_cast<const ITEM_TYPE&>(item).PROPERTY; \
            break;
        ADD(BufferOffset, Buffer, offset)
        ADD(BufferRowCount, Buffer, rowCount)
        ADD(ColumnDataType, Column, dataType)
        ADD(ColumnCount, Column, count)
        ADD(ColumnPadding, Column, padding)
        ADD(TextureTarget, Texture, target)
        ADD(TextureFormat, Texture, format)
        ADD(TextureWidth, Texture, width)
        ADD(TextureHeight, Texture, height)
        ADD(TextureDepth, Texture, depth)
        ADD(TextureLayers, Texture, layers)
        ADD(TextureSamples, Texture, samples)
        ADD(TextureFlipY, Texture, flipY)
        ADD(ImageLevel, Image, level)
        ADD(ImageLayer, Image, layer)
        ADD(ImageFace, Image, face)
        ADD(ShaderType, Shader, type)
        ADD(BindingType, Binding, type)
        ADD(BindingEditor, Binding, editor)
        ADD(BindingValueCount, Binding, valueCount)
        ADD(BindingCurrentValue, Binding, currentValue)
        ADD(AttributeBufferId, Attribute, bufferId)
        ADD(AttributeColumnId, Attribute, columnId)
        ADD(AttributeNormalize, Attribute, normalize)
        ADD(AttributeDivisor, Attribute, divisor)
        ADD(AttachmentTextureId, Attachment, textureId)
        ADD(AttachmentLevel, Attachment, level)
        ADD(AttachmentLayered, Attachment, layered)
        ADD(AttachmentLayer, Attachment, layer)
        ADD(AttachmentBlendColorEq, Attachment, blendColorEq)
        ADD(AttachmentBlendColorSource, Attachment, blendColorSource)
        ADD(AttachmentBlendColorDest, Attachment, blendColorDest)
        ADD(AttachmentBlendAlphaEq, Attachment, blendAlphaEq)
        ADD(AttachmentBlendAlphaSource, Attachment, blendAlphaSource)
        ADD(AttachmentBlendAlphaDest, Attachment, blendAlphaDest)
        ADD(AttachmentColorWriteMask, Attachment, colorWriteMask)
        ADD(AttachmentDepthComparisonFunc, Attachment, depthComparisonFunc)
        ADD(AttachmentDepthOffsetFactor, Attachment, depthOffsetFactor)
        ADD(AttachmentDepthOffsetUnits, Attachment, depthOffsetUnits)
        ADD(AttachmentDepthClamp, Attachment, depthClamp)
        ADD(AttachmentDepthWrite, Attachment, depthWrite)
        ADD(AttachmentStencilFrontComparisonFunc, Attachment, stencilFrontComparisonFunc)
        ADD(AttachmentStencilFrontReference, Attachment, stencilFrontReference)
        ADD(AttachmentStencilFrontReadMask, Attachment, stencilFrontReadMask)
        ADD(AttachmentStencilFrontFailOp, Attachment, stencilFrontFailOp)
        ADD(AttachmentStencilFrontDepthFailOp, Attachment, stencilFrontDepthFailOp)
        ADD(AttachmentStencilFrontDepthPassOp, Attachment, stencilFrontDepthPassOp)
        ADD(AttachmentStencilFrontWriteMask, Attachment, stencilFrontWriteMask)
        ADD(AttachmentStencilBackComparisonFunc, Attachment, stencilBackComparisonFunc)
        ADD(AttachmentStencilBackReference, Attachment, stencilBackReference)
        ADD(AttachmentStencilBackReadMask, Attachment, stencilBackReadMask)
        ADD(AttachmentStencilBackFailOp, Attachment, stencilBackFailOp)
        ADD(AttachmentStencilBackDepthFailOp, Attachment, stencilBackDepthFailOp)
        ADD(AttachmentStencilBackDepthPassOp, Attachment, stencilBackDepthPassOp)
        ADD(AttachmentStencilBackWriteMask, Attachment, stencilBackWriteMask)
        ADD(TargetFrontFace, Target, frontFace)
        ADD(TargetCullMode, Target, cullMode)
        ADD(TargetLogicOperation, Target, logicOperation)
        ADD(TargetBlendConstant, Target, blendConstant)
        ADD(CallType, Call, type)
        ADD(CallProgramId, Call, programId)
        ADD(CallTargetId, Call, targetId)
        ADD(CallVertexStreamId, Call, vertexStreamId)
        ADD(CallPrimitiveType, Call, primitiveType)
        ADD(CallPatchVertices, Call, patchVertices)
        ADD(CallCount, Call, count)
        ADD(CallFirst, Call, first)
        ADD(CallIndexBufferId, Call, indexBufferId)
        ADD(CallBaseVertex, Call, baseVertex)
        ADD(CallInstanceCount, Call, instanceCount)
        ADD(CallBaseInstance, Call, baseInstance)
        ADD(CallIndirectBufferId, Call, indirectBufferId)
        ADD(CallDrawCount, Call, drawCount)
        ADD(CallWorkGroupsX, Call, workGroupsX)
        ADD(CallWorkGroupsY, Call, workGroupsY)
        ADD(CallWorkGroupsZ, Call, workGroupsZ)
        ADD(CallBufferId, Call, bufferId)
        ADD(CallTextureId, Call, textureId)
        ADD(CallClearColor, Call, clearColor)
        ADD(CallClearDepth, Call, clearDepth)
        ADD(CallClearStencil, Call, clearStencil)
#undef ADD

#define ADD(COLUMN_TYPE, PROPERTY) \
        case ColumnType::COLUMN_TYPE: \
            if (item.itemType == ItemType::Binding) { \
                const auto &binding = static_cast<const Binding&>(item); \
                return binding.values[binding.currentValue].PROPERTY; \
            } \
            break;
        ADD(BindingValueFields, fields)
        ADD(BindingValueTextureId, textureId)
        ADD(BindingValueBufferId, bufferId)
        ADD(BindingValueLevel, level)
        ADD(BindingValueLayered, layered)
        ADD(BindingValueLayer, layer)
        ADD(BindingValueMinFilter, minFilter)
        ADD(BindingValueMagFilter, magFilter)
        ADD(BindingValueWrapModeX, wrapModeX)
        ADD(BindingValueWrapModeY, wrapModeY)
        ADD(BindingValueWrapModeZ, wrapModeZ)
        ADD(BindingValueBorderColor, borderColor)
        ADD(BindingValueComparisonFunc, comparisonFunc)
#undef ADD

        case FirstBindingValue:
        case LastBindingValue:
            break;
    }
    return { };
}

bool SessionModel::setData(const QModelIndex &index,
    const QVariant &value, int role)
{
    if (role != Qt::EditRole &&
        role != Qt::CheckStateRole)
        return false;

    auto &item = getItem(index);

    if (role == Qt::CheckStateRole) {
        auto checked = (value == Qt::Checked);
        switch (item.itemType) {
            case ItemType::Call:
                undoableAssignment(index,
                    &static_cast<Call&>(item).checked, checked);
                return true;
            default:
                return false;
        }
    }

    switch (static_cast<ColumnType>(index.column())) {
        case ColumnType::Name:
            undoableAssignment(index, &item.name, value.toString());
            return true;
        case ColumnType::InlineScope:
            undoableAssignment(index, &item.inlineScope, value.toBool());
            return true;

        case ColumnType::FileName:
            if (castItem<FileItem>(item)) {
                undoableFileNameAssignment(index,
                    static_cast<FileItem&>(item), value.toString());
                return true;
            }
            break;

#define ADD(COLUMN_TYPE, ITEM_TYPE, PROPERTY, TO_TYPE) \
        case ColumnType::COLUMN_TYPE: \
            if (item.itemType == ItemType::ITEM_TYPE) { \
                undoableAssignment(index, \
                    &static_cast<ITEM_TYPE&>(item).PROPERTY, value.TO_TYPE()); \
                return true; \
            } \
            break;

        ADD(BufferOffset, Buffer, offset, toInt)
        ADD(BufferRowCount, Buffer, rowCount, toInt)
        ADD(ColumnDataType, Column, dataType, toInt)
        ADD(ColumnCount, Column, count, toInt)
        ADD(ColumnPadding, Column, padding, toInt)
        ADD(TextureTarget, Texture, target, toInt)
        ADD(TextureFormat, Texture, format, toInt)
        ADD(TextureWidth, Texture, width, toInt)
        ADD(TextureHeight, Texture, height, toInt)
        ADD(TextureDepth, Texture, depth, toInt)
        ADD(TextureLayers, Texture, layers, toInt)
        ADD(TextureSamples, Texture, samples, toInt)
        ADD(TextureFlipY, Texture, flipY, toBool)
        ADD(ImageLevel, Image, level, toInt)
        ADD(ImageLayer, Image, layer, toInt)
        ADD(ImageFace, Image, face, toInt)
        ADD(ShaderType, Shader, type, toInt)
        ADD(BindingType, Binding, type, toInt)
        ADD(BindingEditor, Binding, editor, toInt)
        ADD(BindingValueCount, Binding, valueCount, toInt)
        ADD(AttributeBufferId, Attribute, bufferId, toInt)
        ADD(AttributeColumnId, Attribute, columnId, toInt)
        ADD(AttributeNormalize, Attribute, normalize, toBool)
        ADD(AttributeDivisor, Attribute, divisor, toInt)
        ADD(AttachmentTextureId, Attachment, textureId, toInt)
        ADD(AttachmentLevel, Attachment, level, toInt)
        ADD(AttachmentLayered, Attachment, layered, toBool)
        ADD(AttachmentLayer, Attachment, layer, toInt)
        ADD(AttachmentBlendColorEq, Attachment, blendColorEq, toInt)
        ADD(AttachmentBlendColorSource, Attachment, blendColorSource, toInt)
        ADD(AttachmentBlendColorDest, Attachment, blendColorDest, toInt)
        ADD(AttachmentBlendAlphaEq, Attachment, blendAlphaEq, toInt)
        ADD(AttachmentBlendAlphaSource, Attachment, blendAlphaSource, toInt)
        ADD(AttachmentBlendAlphaDest, Attachment, blendAlphaDest, toInt)
        ADD(AttachmentColorWriteMask, Attachment, colorWriteMask, toUInt)
        ADD(AttachmentDepthComparisonFunc, Attachment, depthComparisonFunc, toInt)
        ADD(AttachmentDepthOffsetFactor, Attachment, depthOffsetFactor, toFloat)
        ADD(AttachmentDepthOffsetUnits, Attachment, depthOffsetUnits, toFloat)
        ADD(AttachmentDepthClamp, Attachment, depthClamp, toBool)
        ADD(AttachmentDepthWrite, Attachment, depthWrite, toBool)
        ADD(AttachmentStencilFrontComparisonFunc, Attachment, stencilFrontComparisonFunc, toInt)
        ADD(AttachmentStencilFrontReference, Attachment, stencilFrontReference, toUInt)
        ADD(AttachmentStencilFrontReadMask, Attachment, stencilFrontReadMask, toUInt)
        ADD(AttachmentStencilFrontFailOp, Attachment, stencilFrontFailOp, toInt)
        ADD(AttachmentStencilFrontDepthFailOp, Attachment, stencilFrontDepthFailOp, toInt)
        ADD(AttachmentStencilFrontDepthPassOp, Attachment, stencilFrontDepthPassOp, toInt)
        ADD(AttachmentStencilFrontWriteMask, Attachment, stencilFrontWriteMask, toUInt)
        ADD(AttachmentStencilBackComparisonFunc, Attachment, stencilBackComparisonFunc, toInt)
        ADD(AttachmentStencilBackReference, Attachment, stencilBackReference, toUInt)
        ADD(AttachmentStencilBackReadMask, Attachment, stencilBackReadMask, toUInt)
        ADD(AttachmentStencilBackFailOp, Attachment, stencilBackFailOp, toInt)
        ADD(AttachmentStencilBackDepthFailOp, Attachment, stencilBackDepthFailOp, toInt)
        ADD(AttachmentStencilBackDepthPassOp, Attachment, stencilBackDepthPassOp, toInt)
        ADD(AttachmentStencilBackWriteMask, Attachment, stencilBackWriteMask, toUInt)
        ADD(TargetFrontFace, Target, frontFace, toInt)
        ADD(TargetCullMode, Target, cullMode, toInt)
        ADD(TargetLogicOperation, Target, logicOperation, toInt)
        ADD(TargetBlendConstant, Target, blendConstant, value<QColor>)
        ADD(CallType, Call, type, toInt)
        ADD(CallProgramId, Call, programId, toInt)
        ADD(CallTargetId, Call, targetId, toInt)
        ADD(CallVertexStreamId, Call, vertexStreamId, toInt)
        ADD(CallPrimitiveType, Call, primitiveType, toInt)
        ADD(CallPatchVertices, Call, patchVertices, toInt)
        ADD(CallCount, Call, count, toInt)
        ADD(CallFirst, Call, first, toInt)
        ADD(CallIndexBufferId, Call, indexBufferId, toInt)
        ADD(CallBaseVertex, Call, baseVertex, toInt)
        ADD(CallInstanceCount, Call, instanceCount, toInt)
        ADD(CallBaseInstance, Call, baseInstance, toInt)
        ADD(CallIndirectBufferId, Call, indirectBufferId, toInt)
        ADD(CallDrawCount, Call, drawCount, toInt)
        ADD(CallWorkGroupsX, Call, workGroupsX, toInt)
        ADD(CallWorkGroupsY, Call, workGroupsY, toInt)
        ADD(CallWorkGroupsZ, Call, workGroupsZ, toInt)
        ADD(CallBufferId, Call, bufferId, toInt)
        ADD(CallTextureId, Call, textureId, toInt)
        ADD(CallClearColor, Call, clearColor, value<QColor>)
        ADD(CallClearDepth, Call, clearDepth, toFloat)
        ADD(CallClearStencil, Call, clearStencil, toInt)
#undef ADD

        case ColumnType::BindingCurrentValue:
            if (item.itemType == ItemType::Binding) {
                auto &binding = static_cast<Binding&>(item);
                auto newValue = qBound(0, value.toInt(),
                    static_cast<int>(binding.values.size() - 1));
                if (binding.currentValue != newValue) {
                    binding.currentValue = newValue;
                    emit dataChanged(
                        this->index(&item, ColumnType::FirstBindingValue),
                        this->index(&item, ColumnType::LastBindingValue));
                }
                return true;
            }
            break;

#define ADD(COLUMN_TYPE, PROPERTY, TO_TYPE) \
        case ColumnType::COLUMN_TYPE: \
            if (item.itemType == ItemType::Binding) { \
                auto &binding = static_cast<Binding&>(item); \
                undoableAssignment(index, \
                    &binding.values[binding.currentValue].PROPERTY, \
                    value.TO_TYPE()); \
                return true; \
            } \
            break;
        ADD(BindingValueFields, fields, toStringList)
        ADD(BindingValueTextureId, textureId, toInt)
        ADD(BindingValueBufferId, bufferId, toInt)
        ADD(BindingValueLevel, level, toInt)
        ADD(BindingValueLayered, layered, toBool)
        ADD(BindingValueLayer, layer, toInt)
        ADD(BindingValueMinFilter, minFilter, toInt)
        ADD(BindingValueMagFilter, magFilter, toInt)
        ADD(BindingValueWrapModeX, wrapModeX, toInt)
        ADD(BindingValueWrapModeY, wrapModeY, toInt)
        ADD(BindingValueWrapModeZ, wrapModeZ, toInt)
        ADD(BindingValueBorderColor, borderColor, value<QColor>)
        ADD(BindingValueComparisonFunc, comparisonFunc, toInt)
#undef ADD

        case FirstBindingValue:
        case LastBindingValue:
            break;
    }
    return false;
}

Qt::ItemFlags SessionModel::flags(const QModelIndex &index) const
{
    Q_UNUSED(index);
    auto type = getItemType(index);
    auto flags = Qt::ItemFlags();
    flags |= Qt::ItemIsSelectable;
    flags |= Qt::ItemIsEnabled;
    flags |= Qt::ItemIsEditable;
    flags |= Qt::ItemIsDragEnabled;
    flags |= Qt::ItemIsUserCheckable;
    switch (type) {
        case ItemType::Group:
        case ItemType::Buffer:
        case ItemType::Texture:
        case ItemType::Program:
        case ItemType::VertexStream:
        case ItemType::Target:
            flags |= Qt::ItemIsDropEnabled;
            break;
        default:
            break;
    }
    // workaround to optimize D&D (do not snap to not droppable targets)
    if (!mDraggedIndices.empty() && (flags & Qt::ItemIsDropEnabled))
        if (!canContainType(index, getItemType(mDraggedIndices.first())))
            flags &= ~Qt::ItemIsDropEnabled;

    return flags;
}

bool SessionModel::removeRows(int row, int count, const QModelIndex &parent)
{
    for (auto i = count - 1; i >= 0; --i)
        deleteItem(index(row + i, 0, parent));
    return true;
}

void SessionModel::insertItem(Item *item, const QModelIndex &parent, int row)
{
    Q_ASSERT(item && !item->parent);
    auto &parentItem = getItem(parent);
    if (row < 0 || row > parentItem.items.size())
        row = parentItem.items.size();

    item->parent = &parentItem;

    if (!item->id)
        item->id = mNextItemId;
    mNextItemId = std::max(mNextItemId, item->id + 1);

    undoableInsertItem(&parentItem.items, item, parent, row);
}

QModelIndex SessionModel::insertItem(ItemType type, QModelIndex parent,
    int row, ItemId id)
{
    // insert as sibling, when parent cannot contain an item of type
    while (!canContainType(parent, type)) {
        if (!parent.isValid())
            return { };
        row = parent.row() + 1;
        parent = parent.parent();
    }

    auto insert = [&](auto item) {
        item->itemType = type;
        item->name = getTypeName(type);
        item->id = id;
        insertItem(item, parent, row);
        return index(item);
    };

    switch (type) {
        case ItemType::Group: return insert(new Group());
        case ItemType::Buffer: return insert(new Buffer());
        case ItemType::Column: return insert(new Column());
        case ItemType::Texture: return insert(new Texture());
        case ItemType::Image: return insert(new Image());
        case ItemType::Program: return insert(new Program());
        case ItemType::Shader: return insert(new Shader());
        case ItemType::Binding: return insert(new Binding());
        case ItemType::VertexStream: return insert(new VertexStream());
        case ItemType::Attribute: return insert(new Attribute());
        case ItemType::Target: return insert(new Target());
        case ItemType::Attachment: return insert(new Attachment());
        case ItemType::Call: return insert(new Call());
        case ItemType::Script: return insert(new Script());
    }
    return { };
}

void SessionModel::deleteItem(const QModelIndex &index)
{
    for (auto i = rowCount(index) - 1; i >= 0; --i)
        deleteItem(this->index(i, 0, index));

    if (index.isValid()) {
        auto &item = getItem(index);
        undoableRemoveItem(&item.parent->items, &item, index);
    }
}

const Item* SessionModel::findItem(ItemId id) const
{
    // TODO: improve performance
    const Item* result = nullptr;
    forEachItem([&](const Item &item) {
        if (item.id == id)
            result = &item;
    });
    return result;
}

QString SessionModel::findItemName(ItemId id) const
{
    if (auto item = findItem(id))
        return item->name;
    return { };
}

void SessionModel::setActiveItems(QSet<ItemId> itemIds)
{
    if (mActiveItemIds == itemIds)
        return;

    mActiveItemIds = std::move(itemIds);
    emit dataChanged(index(0, 0), index(rowCount(), 0),
        { Qt::FontRole, Qt::ForegroundRole });
}

void SessionModel::setItemActive(ItemId id, bool active)
{
    if (active == mActiveItemIds.contains(id))
        return;

    if (active)
        mActiveItemIds.insert(id);
    else
        mActiveItemIds.remove(id);

    auto item = findItem(id);
    emit dataChanged(index(item), index(item),
        { Qt::FontRole, Qt::ForegroundRole });
}

QModelIndex SessionModel::index(const Item *item, int column) const
{
    Q_ASSERT(item);
    if (item == mRoot.data())
        return { };

    auto itemPtr = const_cast<Item*>(item);
    return createIndex(item->parent->items.indexOf(itemPtr), column, itemPtr);
}

const Item &SessionModel::getItem(const QModelIndex &index) const
{
    if (index.isValid())
        return *static_cast<const Item*>(index.internalPointer());
    return *mRoot;
}

Item &SessionModel::getItem(const QModelIndex &index)
{
    if (index.isValid())
        return *static_cast<Item*>(index.internalPointer());
    return *mRoot;
}

ItemId SessionModel::getItemId(const QModelIndex &index) const
{
    return getItem(index).id;
}

ItemType SessionModel::getItemType(const QModelIndex &index) const
{
    return getItem(index).itemType;
}

void SessionModel::insertItem(QList<Item*> *list, Item *item,
        const QModelIndex &parent, int row)
{
    beginInsertRows(parent, row, row);
    list->insert(row, item);
    endInsertRows();
}

void SessionModel::removeItem(QList<Item*> *list,
    const QModelIndex &parent, int row)
{
    beginRemoveRows(parent, row, row);
    list->removeAt(row);
    endRemoveRows();
}

void SessionModel::undoableInsertItem(QList<Item*> *list, Item *item,
    const QModelIndex &parent, int row)
{
    mUndoStack.push(makeUndoCommand("Insert",
        [=](){ insertItem(list, item, parent, row); },
        [=](){ removeItem(list, parent, row); },
        [=](){ delete item; },
        true));
}

void SessionModel::undoableRemoveItem(QList<Item*> *list, Item *item,
    const QModelIndex &index)
{
    mUndoStack.push(makeUndoCommand("Remove",
        [=](){ removeItem(list, index.parent(), index.row()); },
        [=](){ insertItem(list, item, index.parent(), index.row()); },
        [=](){ delete item; },
        false));
}

template<typename T, typename S>
void SessionModel::assignment(const QModelIndex &index, T *to, S &&value)
{
    Q_ASSERT(index.isValid());
    if (*to != value) {
        *to = value;
        emit dataChanged(index, index);
    }
}

template<typename T, typename S>
void SessionModel::undoableAssignment(const QModelIndex &index, T *to,
    S &&value, int mergeId)
{
    Q_ASSERT(index.isValid());
    if (*to != value) {
        if (mergeId < 0)
            mergeId = -index.column();
        mergeId += reinterpret_cast<uintptr_t>(index.internalPointer());

        mUndoStack.push(new MergingUndoCommand(mergeId,
            makeUndoCommand("Edit ",
                [=, value = std::forward<S>(value)]()
                { *to = static_cast<T>(value); emit dataChanged(index, index); },
                [=, orig = *to]()
                { *to = orig; emit dataChanged(index, index); },
                [](){})));
    }
}

void SessionModel::undoableFileNameAssignment(const QModelIndex &index,
    FileItem &item, QString fileName)
{
    if (item.fileName != fileName) {
        mUndoStack.beginMacro("Edit");

        if (item.name == getTypeName(item.itemType) ||
            item.name == FileDialog::getFileTitle(item.fileName)) {

            auto newFileName =
                (FileDialog::isEmptyOrUntitled(fileName) ?
                    getTypeName(item.itemType) : fileName);

            setData(this->index(&item, Name),
                FileDialog::getFileTitle(newFileName));
        }
        undoableAssignment(index, &item.fileName, fileName);
        mUndoStack.endMacro();
    }
}

QStringList SessionModel::mimeTypes() const
{
    return { MimeType };
}

Qt::DropActions SessionModel::supportedDragActions() const
{
    return Qt::CopyAction | Qt::MoveAction;
}

Qt::DropActions SessionModel::supportedDropActions() const
{
    return Qt::CopyAction | Qt::MoveAction;
}

bool SessionModel::canDropMimeData(const QMimeData *data,
        Qt::DropAction action, int row, int column,
        const QModelIndex &parent) const
{
    Q_UNUSED(row);
    Q_UNUSED(column);
    if (!QAbstractItemModel::canDropMimeData(
            data, action, row, column, parent))
        return false;

    QXmlStreamReader xml(data->data(MimeType));
    while (xml.readNextStartElement()) {
        if (xml.name() == MultipleTag)
            continue;

        if (!canContainType(parent, typeByTagName(xml.name())))
            return false;

        xml.skipCurrentElement();
    }
    return true;
}

void SessionModel::clear()
{
    mUndoStack.clear();
    mUndoStack.setUndoLimit(1);

    deleteItem(QModelIndex());

    mUndoStack.clear();
    mUndoStack.setUndoLimit(0);
}

bool SessionModel::save(const QString &fileName)
{
    QDir::setCurrent(QFileInfo(fileName).path());
    QScopedPointer<QMimeData> mime(mimeData({ QModelIndex() }));
    if (!mime)
        return false;

    QFile file(fileName);
    if (!file.open(QIODevice::WriteOnly))
        return false;

    file.write(mime->data(MimeType));
    file.close();

    mUndoStack.setClean();
    return true;
}

bool SessionModel::load(const QString &fileName)
{
    QFile file(fileName);
    if (!file.open(QIODevice::ReadOnly))
        return false;

    QDir::setCurrent(QFileInfo(fileName).path());
    QMimeData data;
    data.setData(MimeType, file.readAll());
    if (!canDropMimeData(&data, Qt::CopyAction, rowCount(), 0, {}))
        return false;

    mUndoStack.clear();
    mUndoStack.setUndoLimit(1);

    dropMimeData(&data, Qt::CopyAction, 0, 0, {});

    mUndoStack.clear();
    mUndoStack.setUndoLimit(0);
    return true;
}

QMimeData *SessionModel::mimeData(const QModelIndexList &indexes) const
{
    auto buffer = QByteArray();
    QXmlStreamWriter xml(&buffer);
    xml.setAutoFormatting(true);
    xml.writeStartDocument();
    if (indexes.size() > 1)
        xml.writeStartElement(MultipleTag);

    const auto serializingWholeSession =
        (indexes.size() == 1 && !indexes.first().isValid());
    const auto relativeFilePaths = serializingWholeSession;

    foreach (QModelIndex index, indexes)
        serialize(xml, getItem(index), relativeFilePaths);

    if (indexes.size() > 1)
        xml.writeEndElement();
    xml.writeEndDocument();
    auto data = new QMimeData();
    data->setData(MimeType, buffer);

    mDraggedIndices = indexes;
    return data;
}

bool SessionModel::dropMimeData(const QMimeData *data, Qt::DropAction action,
        int row, int column, const QModelIndex &parent)
{
    Q_UNUSED(column);
    if (action == Qt::IgnoreAction)
        return true;

    if (row < 0)
        row = rowCount(parent);

    mUndoStack.beginMacro("Move");

    if (action == Qt::MoveAction && !mDraggedIndices.empty()) {
        std::stable_sort(mDraggedIndices.begin(), mDraggedIndices.end(),
            [](const auto &a, const auto &b) { return a.row() > b.row(); });
        for (auto index : mDraggedIndices) {
            if (index.parent() == parent &&
                index.row() < row)
                --row;
            deleteItem(index);
        }
    }

    QXmlStreamReader xml(data->data(MimeType));
    while (xml.readNextStartElement()) {
        if (xml.name() == MultipleTag)
            continue;

        if (canContainType(parent, typeByTagName(xml.name())))
            deserialize(xml, parent, row++);
        else
            xml.skipCurrentElement();
    }

    // fixup item references
    foreach (ItemId prevId, mDroppedIdsReplaced.keys())
        foreach (ItemId* reference, mDroppedReferences)
            if (*reference == prevId)
                *reference = mDroppedIdsReplaced[prevId];
    mDroppedIdsReplaced.clear();
    mDroppedReferences.clear();
    mDraggedIndices.clear();

    mUndoStack.endMacro();
    return true;
}

void SessionModel::serialize(QXmlStreamWriter &xml, const Item &item,
    bool relativeFilePaths) const
{
    xml.writeStartElement(tagNameByType(item.itemType));

    const auto writeString = [&](const char *name, const auto &property) {
        xml.writeAttribute(name, property);
    };
    const auto writeFileName = [&](const char *name, const auto &property) {
        if (!FileDialog::isUntitled(property))
            writeString(name, relativeFilePaths ?
                QDir::current().relativeFilePath(property) : property);
    };
    const auto write = [&](const char *name, const auto &property) {
        xml.writeAttribute(name, QString::number(property));
    };
    const auto writeRef= [&](const char *name, const auto &property) {
        if (property)
            xml.writeAttribute(name, QString::number(property));
    };
    const auto writeBool = [&](const char *name, const auto &property) {
        xml.writeAttribute(name, (property ? "true" : "false"));
    };

    if (item.id)
        write("id", item.id);
    if (!item.name.isEmpty())
        writeString("name", item.name);

    switch (item.itemType) {
        case ItemType::Group: {
            const auto &group = static_cast<const Group&>(item);
            writeBool("inlineScope", group.inlineScope);
            break;
        }

        case ItemType::Buffer: {
            const auto &buffer = static_cast<const Buffer&>(item);
            writeFileName("fileName", buffer.fileName);
            write("offset", buffer.offset);
            write("rowCount", buffer.rowCount);
            break;
        }

        case ItemType::Column: {
            const auto &column = static_cast<const Column&>(item);
            write("dataType", column.dataType);
            write("count", column.count);
            write("padding", column.padding);
            break;
        }

        case ItemType::Texture: {
            const auto &texture = static_cast<const Texture&>(item);
            auto kind = getKind(texture);
            writeFileName("fileName", texture.fileName);
            write("target", texture.target);
            write("format",  texture.format);
            write("width",  texture.width);
            if (kind.dimensions > 1)
                write("height",  texture.height);
            if (kind.dimensions > 2)
                write("depth", texture.depth);
            if (kind.array)
                write("layers", texture.layers);
            if (kind.multisample)
                write("samples", texture.samples);
            if (kind.dimensions > 1 && texture.flipY)
                writeBool("flipY", texture.flipY);
            break;
        }

        case ItemType::Image: {
            const auto &image = static_cast<const Image&>(item);
            writeFileName("fileName", image.fileName);
            write("level", image.level);
            write("layer", image.layer);
            write("face", image.face);
            break;
        }

        case ItemType::Program: {
            break;
        }

        case ItemType::Shader: {
            const auto &shader = static_cast<const Shader&>(item);
            write("type", shader.type);
            writeFileName("fileName", shader.fileName);
            break;
        }

        case ItemType::Binding: {
            const auto &binding = static_cast<const Binding&>(item);
            write("type", binding.type);
            if (binding.type == Binding::Uniform)
                write("editor", binding.editor);
            for (auto i = 0; i < binding.valueCount; ++i) {
                const auto &value = binding.values[i];
                xml.writeStartElement(ValueTag);
                switch (binding.type) {
                    case Binding::Uniform:
                        foreach (const QString &field, value.fields)
                            xml.writeTextElement(FieldTag, field);
                        break;
                    case Binding::Image:
                        writeRef("textureId", value.textureId);
                        write("level", value.level);
                        writeBool("layered", value.layered);
                        write("layer", value.layer);
                        break;

                    case Binding::Sampler:
                        writeRef("textureId", value.textureId);
                        write("minFilter", value.minFilter);
                        write("magFilter", value.magFilter);
                        write("wrapModeX", value.wrapModeX);
                        write("wrapModeY", value.wrapModeY);
                        write("wrapModeZ", value.wrapModeZ);
                        break;

                    case Binding::Buffer:
                        writeRef("bufferId", value.bufferId);
                        break;
                }
                xml.writeEndElement();
            }
            break;
        }

        case ItemType::VertexStream: {
            break;
        }

        case ItemType::Attribute: {
            const auto &attribute = static_cast<const Attribute&>(item);
            writeRef("bufferId", attribute.bufferId);
            writeRef("columnId", attribute.columnId);
            writeBool("normalize", attribute.normalize);
            write("divisor", attribute.divisor);
            break;
        }

        case ItemType::Target: {
            const auto &target = static_cast<const Target&>(item);
            write("frontFace", target.frontFace);
            write("cullMode", target.cullMode);
            write("logicOperation", target.logicOperation);
            writeString("blendConstant", target.blendConstant.name());
            break;
        }

        case ItemType::Attachment: {
            const auto &attachment = static_cast<const Attachment&>(item);
            auto kind = TextureKind();
            if (auto texture = findItem<Texture>(attachment.textureId))
                kind = getKind(*texture);

            writeRef("textureId", attachment.textureId);
            write("level", attachment.level);
            if (kind.array) {
              writeBool("layered", attachment.layered);
              write("layer", attachment.layer);
            }
            if (kind.color) {
                write("blendColorEq", attachment.blendColorEq);
                write("blendColorSource", attachment.blendColorSource);
                write("blendColorDest", attachment.blendColorDest);
                write("blendAlphaEq", attachment.blendAlphaEq);
                write("blendAlphaSource", attachment.blendAlphaSource);
                write("blendAlphaDest", attachment.blendAlphaDest);
                write("colorWriteMask", attachment.colorWriteMask);
            }
            if (kind.depth) {
                write("depthComparisonFunc", attachment.depthComparisonFunc);
                write("depthOffsetFactor", attachment.depthOffsetFactor);
                write("depthOffsetUnits", attachment.depthOffsetUnits);
                write("depthClamp", attachment.depthClamp);
                writeBool("depthWrite", attachment.depthWrite);
            }
            if (kind.stencil) {
                write("stencilFrontComparisonFunc", attachment.stencilFrontComparisonFunc);
                write("stencilFrontReference", attachment.stencilFrontReference);
                write("stencilFrontReadMask", attachment.stencilFrontReadMask);
                write("stencilFrontFailOp", attachment.stencilFrontFailOp);
                write("stencilFrontDepthFailOp", attachment.stencilFrontDepthFailOp);
                write("stencilFrontDepthPassOp", attachment.stencilFrontDepthPassOp);
                write("stencilFrontWriteMask", attachment.stencilFrontWriteMask);
                write("stencilBackComparisonFunc", attachment.stencilBackComparisonFunc);
                write("stencilBackReference", attachment.stencilBackReference);
                write("stencilBackReadMask", attachment.stencilBackReadMask);
                write("stencilBackFailOp", attachment.stencilBackFailOp);
                write("stencilBackDepthFailOp", attachment.stencilBackDepthFailOp);
                write("stencilBackDepthPassOp", attachment.stencilBackDepthPassOp);
                write("stencilBackWriteMask", attachment.stencilBackWriteMask);
            }
            break;
        }

        case ItemType::Call: {
            const auto &call = static_cast<const Call&>(item);
            const auto kind = getKind(call);
            const auto type = call.type;

            writeBool("checked", call.checked);
            write("type", call.type);
            if (kind.draw || kind.compute)
                writeRef("programId", call.programId);
            if (kind.draw) {
                writeRef("targetId", call.targetId);
                writeRef("vertexStreamId", call.vertexStreamId);
                write("primitiveType", call.primitiveType);
            }
            if (kind.drawPatches)
                writeRef("patchVertices", call.patchVertices);
            if (kind.drawIndexed)
                writeRef("indexBufferId", call.indexBufferId);
            if (kind.drawDirect) {
                write("count", call.count);
                write("first", call.first);
                write("instanceCount", call.instanceCount);
                write("baseInstance", call.baseInstance);
                if (kind.drawIndexed)
                    write("baseVertex", call.baseVertex);
            }
            if (kind.drawIndirect) {
                writeRef("indirectBufferId", call.indirectBufferId);
                write("drawCount", call.drawCount);
            }
            if (kind.compute) {
                write("workGroupsX", call.workGroupsX);
                write("workGroupsY", call.workGroupsY);
                write("workGroupsZ", call.workGroupsZ);
            }
            if (type == Call::ClearTexture || type == Call::GenerateMipmaps)
                writeRef("textureId", call.textureId);
            if (type == Call::ClearTexture) {
                writeString("clearColor", call.clearColor.name());
                write("clearDepth", call.clearDepth);
                write("clearStencil", call.clearStencil);
            }
            if (type == Call::ClearBuffer)
                writeRef("bufferId", call.bufferId);
            break;
        }

        case ItemType::Script: {
            const auto &script = static_cast<const Script&>(item);
            writeFileName("fileName", script.fileName);
            break;
        }
    }

    foreach (const Item* item, item.items)
        serialize(xml, *item, relativeFilePaths);

    xml.writeEndElement();
}

void SessionModel::deserialize(QXmlStreamReader &xml,
    const QModelIndex &parent, int row)
{
    const auto type = typeByTagName(xml.name());

    // root is a group without name, do not add another level
    if (type == ItemType::Group && xml.attributes().value("name").isEmpty()) {
        while (xml.readNextStartElement())
            deserialize(xml, parent, row++);
        return;
    }
    if (!canContainType(parent, type)) {
        xml.skipCurrentElement();
        return;
    }

    const auto readString = [&](const char* name, auto &property) {
        property = xml.attributes().value(name).toString();
    };
    const auto readFileName = [&](const char* name, auto &property) {
        readString(name, property);
        if (!property.isEmpty())
            property = QDir::current().absoluteFilePath(property);
    };
    const auto read = [&](const char* name, auto &property) {
        using Type = std::decay_t<decltype(property)>;
        auto value = xml.attributes().value(name);
        if (!value.isNull())
            property = static_cast<Type>(value.toDouble());
    };
    const auto readRef = [&](const char* name, ItemId &id) {
        auto value = xml.attributes().value(name);
        if (!value.isNull()) {
            id = static_cast<ItemId>(value.toInt());
            mDroppedReferences.append(&id);
        }
    };
    const auto readEnum = [&](const char* name, auto &property) {
        using Type = std::decay_t<decltype(property)>;
        auto value = xml.attributes().value(name);
        if (!value.isNull())
            property = static_cast<Type>(value.toInt());
    };
    const auto readBool = [&](const char* name, auto &property) {
        auto value = xml.attributes().value(name);
        if (!value.isNull())
            property = (value.toString() == "true");
    };

    auto id = xml.attributes().value("id").toInt();
    if (findItem(id)) {
        // generate new id when it collides with an existing item
        auto prevId = std::exchange(id, mNextItemId++);
        mDroppedIdsReplaced.insert(prevId, id);
    }

    const auto index = insertItem(type, parent, row, id);
    auto &item = getItem(index);
    readString("name", item.name);

    switch (item.itemType) {
        case ItemType::Group: {
            auto &group = static_cast<Group&>(item);
            readBool("inlineScope", group.inlineScope);
            break;
        }

        case ItemType::Buffer: {
            auto &buffer = static_cast<Buffer&>(item);
            readFileName("fileName", buffer.fileName);
            read("offset", buffer.offset);
            read("rowCount", buffer.rowCount);
            break;
        }

        case ItemType::Column: {
            auto &column = static_cast<Column&>(item);
            readEnum("dataType", column.dataType);
            read("count", column.count);
            read("padding", column.padding);
            break;
        }

        case ItemType::Texture: {
            auto &texture = static_cast<Texture&>(item);
            readFileName("fileName", texture.fileName);
            readEnum("target", texture.target);
            readEnum("format", texture.format);
            read("width", texture.width);
            read("height", texture.height);
            read("depth", texture.depth);
            read("layers", texture.layers);
            read("samples", texture.samples);
            readBool("flipY", texture.flipY);
            break;
        }

        case ItemType::Image: {
            auto &image = static_cast<Image&>(item);
            readFileName("fileName", image.fileName);
            read("level", image.level);
            read("layer", image.layer);
            readEnum("face", image.face);
            break;
        }

        case ItemType::Program: {
            break;
        }

        case ItemType::Shader: {
            auto &shader = static_cast<Shader&>(item);
            readEnum("type", shader.type);
            readFileName("fileName", shader.fileName);
            break;
        }

        case ItemType::Binding: {
            auto &binding = static_cast<Binding&>(item);
            readEnum("type", binding.type);
            readEnum("editor", binding.editor);
            binding.valueCount = 0;
            while (xml.readNextStartElement()) {
                binding.valueCount = qMin(binding.valueCount + 1,
                    static_cast<int>(binding.values.size()));
                auto &value = binding.values[binding.valueCount - 1];
                readRef("textureId", value.textureId);
                readRef("bufferId", value.bufferId);
                read("level", value.level);
                readBool("layered", value.layered);
                read("layer", value.layer);
                readEnum("minFilter", value.minFilter);
                readEnum("magFilter", value.magFilter);
                readEnum("wrapModeX", value.wrapModeX);
                readEnum("wrapModeY", value.wrapModeY);
                readEnum("wrapModeZ", value.wrapModeZ);
                while (xml.readNextStartElement())
                    value.fields.append(xml.readElementText());
            }
            return;
        }

        case ItemType::VertexStream: {
            break;
        }

        case ItemType::Attribute: {
            auto &attribute = static_cast<Attribute&>(item);
            readRef("bufferId", attribute.bufferId);
            readRef("columnId", attribute.columnId);
            readBool("normalize", attribute.normalize);
            read("divisor", attribute.divisor);
            break;
        }

        case ItemType::Target: {
            auto &target = static_cast<Target&>(item);
            readEnum("frontFace", target.frontFace);
            readEnum("cullMode", target.cullMode);
            readEnum("logicOperation", target.logicOperation);
            readString("blendConstant", target.blendConstant);
            break;
        }

        case ItemType::Attachment: {
            auto &attachment = static_cast<Attachment&>(item);
            readRef("textureId", attachment.textureId);
            read("level", attachment.level);
            readBool("layered", attachment.layered);
            read("layer", attachment.layer);
            readEnum("blendColorEq", attachment.blendColorEq);
            readEnum("blendColorSource", attachment.blendColorSource);
            readEnum("blendColorDest", attachment.blendColorDest);
            readEnum("blendAlphaEq", attachment.blendAlphaEq);
            readEnum("blendAlphaSource", attachment.blendAlphaSource);
            readEnum("blendAlphaDest", attachment.blendAlphaDest);
            read("colorWriteMask", attachment.colorWriteMask);
            readEnum("depthComparisonFunc", attachment.depthComparisonFunc);
            read("depthOffsetFactor", attachment.depthOffsetFactor);
            read("depthOffsetUnits", attachment.depthOffsetUnits);
            read("depthClamp", attachment.depthClamp);
            readBool("depthWrite", attachment.depthWrite);
            readEnum("stencilFrontComparisonFunc", attachment.stencilFrontComparisonFunc);
            read("stencilFrontReference", attachment.stencilFrontReference);
            read("stencilFrontReadMask", attachment.stencilFrontReadMask);
            readEnum("stencilFrontFailOp", attachment.stencilFrontFailOp);
            readEnum("stencilFrontDepthFailOp", attachment.stencilFrontDepthFailOp);
            readEnum("stencilFrontDepthPassOp", attachment.stencilFrontDepthPassOp);
            read("stencilFrontWriteMask", attachment.stencilFrontWriteMask);
            readEnum("stencilBackComparisonFunc", attachment.stencilBackComparisonFunc);
            read("stencilBackReference", attachment.stencilBackReference);
            read("stencilBackReadMask", attachment.stencilBackReadMask);
            readEnum("stencilBackFailOp", attachment.stencilBackFailOp);
            readEnum("stencilBackDepthFailOp", attachment.stencilBackDepthFailOp);
            readEnum("stencilBackDepthPassOp", attachment.stencilBackDepthPassOp);
            read("stencilBackWriteMask", attachment.stencilBackWriteMask);
            break;
        }

        case ItemType::Call: {
            auto &call = static_cast<Call&>(item);
            readBool("checked", call.checked);
            readEnum("type", call.type);
            readRef("programId", call.programId);
            readRef("vertexStreamId", call.vertexStreamId);
            readRef("framebufferId", call.targetId); // TODO: remove
            readRef("targetId", call.targetId);
            readRef("indexBufferId", call.indexBufferId);
            readEnum("primitiveType", call.primitiveType);
            read("patchVertices", call.patchVertices);
            read("count", call.count);
            read("first", call.first);
            read("baseVertex", call.baseVertex);
            read("instanceCount", call.instanceCount);
            read("baseInstance", call.baseInstance);
            readRef("indirectBufferId", call.indirectBufferId);
            read("drawCount", call.drawCount);
            read("workGroupsX", call.workGroupsX);
            read("workGroupsY", call.workGroupsY);
            read("workGroupsZ", call.workGroupsZ);
            readRef("textureId", call.textureId);
            readString("clearColor", call.clearColor);
            read("clearDepth", call.clearDepth);
            read("clearStencil", call.clearStencil);
            readRef("bufferId", call.bufferId);
            break;
        }

        case ItemType::Script: {
            auto &texture = static_cast<Texture&>(item);
            readFileName("fileName", texture.fileName);
            break;
        }
    }

    while (xml.readNextStartElement())
        deserialize(xml, index, -1);
}
