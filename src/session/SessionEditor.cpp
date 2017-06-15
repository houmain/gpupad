#include "SessionEditor.h"
#include "Singletons.h"
#include "SessionModel.h"
#include "EditActions.h"
#include "FileDialog.h"
#include <QApplication>
#include <QClipboard>
#include <QMenu>

SessionEditor::SessionEditor(QWidget *parent)
    : QTreeView(parent)
    , mModel(Singletons::sessionModel())
    , mContextMenu(new QMenu(this))
{
    setModel(&mModel);
    setFrameShape(QFrame::NoFrame);
    setHeaderHidden(true);
    setUniformRowHeights(true);
    setStyleSheet("QTreeView::item { padding: 1px; }");
    setAnimated(true);
    setVerticalScrollMode(QTreeView::ScrollPerPixel);
    setDragEnabled(true);
    setDefaultDropAction(Qt::MoveAction);
    setDragDropMode(QTreeView::DragDrop);
    setSelectionMode(QAbstractItemView::ExtendedSelection);
    setExpandsOnDoubleClick(false);
    setEditTriggers(QTreeView::EditKeyPressed | QTreeView::SelectedClicked);
    setContextMenuPolicy(Qt::CustomContextMenu);
    setAutoExpandDelay(750);
    setFileName({ });

    mRenameAction = new QAction(tr("&Rename"), this);
    mRenameAction->setShortcut(QKeySequence("F2"));

    mActivateAction = new QAction(QIcon(":/images/16x16/system-run.png"),
        tr("&Activate"), this);

    connect(&mModel.undoStack(), &QUndoStack::cleanChanged,
        [this](bool clean) { emit modificationChanged(!clean); });
    connect(this, &QTreeView::activated,
        this, &SessionEditor::treeItemActivated);
    connect(this, &QTreeView::customContextMenuRequested,
        this, &SessionEditor::openContextMenu);
    connect(mActivateAction, &QAction::triggered,
        this, &SessionEditor::activateCurrentItem);
    connect(mRenameAction, &QAction::triggered,
        this, &SessionEditor::renameCurrentItem);

    auto addAction = [&](auto &action, auto type) {
        action = new QAction(mModel.getTypeIcon(type),
            tr("Add &") + mModel.getTypeName(type), this);
        connect(action, &QAction::triggered, [this, type]() { addItem(type); });
    };
    addAction(mAddGroupAction, ItemType::Group);
    addAction(mAddBufferAction, ItemType::Buffer);
    addAction(mAddColumnAction, ItemType::Column);
    addAction(mAddTextureAction, ItemType::Texture);
    addAction(mAddImageAction, ItemType::Image);
    addAction(mAddSamplerAction, ItemType::Sampler);
    addAction(mAddProgramAction, ItemType::Program);
    addAction(mAddShaderAction, ItemType::Shader);
    addAction(mAddBindingAction, ItemType::Binding);
    addAction(mAddPrimitivesAction, ItemType::Primitives);
    addAction(mAddAttributeAction, ItemType::Attribute);
    addAction(mAddFramebufferAction, ItemType::Framebuffer);
    addAction(mAddAttachmentAction, ItemType::Attachment);
    addAction(mAddCallAction, ItemType::Call);
    addAction(mAddStateAction, ItemType::State);
    addAction(mAddScriptAction, ItemType::Script);
}

void SessionEditor::mouseReleaseEvent(QMouseEvent *event)
{
    QTreeView::mouseReleaseEvent(event);

    // keep current selected
    if (selectionModel()->selection().isEmpty() &&
        currentIndex().isValid())
        selectionModel()->select(
            currentIndex(), QItemSelectionModel::Select);
}

void SessionEditor::selectionChanged(const QItemSelection &selected,
                                     const QItemSelection &deselected)
{
    // do not allow to select item with different parents
    const auto parent = currentIndex().parent();
    const auto selection = selectionModel()->selection();
    auto invalid = QItemSelection();
    foreach (QModelIndex index, selection.indexes())
        if (index.parent() != parent)
            invalid.select(index, index);

    if (!invalid.empty())
        selectionModel()->select(invalid, QItemSelectionModel::Deselect);
    else
        QTreeView::selectionChanged(selected, deselected);
}

void SessionEditor::focusInEvent(QFocusEvent *event)
{
    QTreeView::focusInEvent(event);
    emit focusChanged(true);
}

void SessionEditor::focusOutEvent(QFocusEvent *event)
{
    QTreeView::focusOutEvent(event);
    emit focusChanged(false);
}

QList<QMetaObject::Connection> SessionEditor::connectEditActions(
    const EditActions &actions, bool focused)
{
    auto c = QList<QMetaObject::Connection>();
    actions.windowFileName->setText(fileName());
    actions.windowFileName->setEnabled(isModified());
    c += connect(this, &SessionEditor::fileNameChanged,
        actions.windowFileName, &QAction::setText);
    c += connect(this, &SessionEditor::modificationChanged,
        actions.windowFileName, &QAction::setEnabled);

    actions.undo->setEnabled(mModel.undoStack().canUndo());
    actions.redo->setEnabled(mModel.undoStack().canRedo());
    c += connect(actions.undo, &QAction::triggered,
        &mModel.undoStack(), &QUndoStack::undo);
    c += connect(actions.redo, &QAction::triggered,
        &mModel.undoStack(), &QUndoStack::redo);
    c += connect(&mModel.undoStack(), &QUndoStack::canUndoChanged,
        actions.undo, &QAction::setEnabled);
    c += connect(&mModel.undoStack(), &QUndoStack::canRedoChanged,
        actions.redo, &QAction::setEnabled);

    // do not enable copy/paste... actions when a property editor is focused
    if (!focused)
        return c;

    const auto hasSelection = currentIndex().isValid();
    actions.cut->setEnabled(hasSelection);
    actions.copy->setEnabled(hasSelection);
    actions.delete_->setEnabled(hasSelection);
    actions.paste->setEnabled(canPaste());
    c += connect(actions.cut, &QAction::triggered,
        this, &SessionEditor::cut);
    c += connect(actions.copy, &QAction::triggered,
        this, &SessionEditor::copy);
    c += connect(actions.paste, &QAction::triggered,
        this, &SessionEditor::paste);
    c += connect(actions.delete_, &QAction::triggered,
        this, &SessionEditor::delete_);

    auto updateEditActions = [this, actions]() {
        actions.cut->setEnabled(hasFocus() && currentIndex().isValid());
        actions.copy->setEnabled(hasFocus() && currentIndex().isValid());
        actions.delete_->setEnabled(hasFocus() && currentIndex().isValid());
        actions.paste->setEnabled(hasFocus() && canPaste());
    };
    c += connect(selectionModel(), &QItemSelectionModel::currentChanged,
        updateEditActions);
    c += connect(this, &SessionEditor::focusChanged, updateEditActions);

    mContextMenu->clear();
    mContextMenu->addAction(actions.undo);
    mContextMenu->addAction(actions.redo);
    mContextMenu->addSeparator();
    mContextMenu->addAction(actions.cut);
    mContextMenu->addAction(actions.copy);
    mContextMenu->addAction(actions.paste);
    mContextMenu->addAction(actions.delete_);
    mContextMenu->addSeparator();
    mContextMenu->addAction(mRenameAction);
    mContextMenu->addAction(mActivateAction);
    mContextMenu->addSeparator();
    mContextMenu->addAction(mAddGroupAction);
    mContextMenu->addAction(mAddBufferAction);
    mContextMenu->addAction(mAddColumnAction);
    mContextMenu->addAction(mAddPrimitivesAction);
    mContextMenu->addAction(mAddAttributeAction);
    mContextMenu->addAction(mAddTextureAction);
    mContextMenu->addAction(mAddImageAction);
    mContextMenu->addAction(mAddSamplerAction);
    mContextMenu->addAction(mAddFramebufferAction);
    mContextMenu->addAction(mAddAttachmentAction);
    mContextMenu->addAction(mAddScriptAction);
    mContextMenu->addAction(mAddBindingAction);
    mContextMenu->addAction(mAddProgramAction);
    mContextMenu->addAction(mAddShaderAction);
    mContextMenu->addAction(mAddStateAction);
    mContextMenu->addAction(mAddCallAction);
    return c;
}

bool SessionEditor::isModified() const
{
    return !mModel.undoStack().isClean();
}

bool SessionEditor::clear()
{
    mModel.clear();
    setFileName({ });
    return true;
}

void SessionEditor::setFileName(const QString &fileName)
{
    mFileName = fileName;
    if (mFileName.isEmpty())
        mFileName = FileDialog::getUntitledSessionFileName();
    emit fileNameChanged(mFileName);
}

bool SessionEditor::load()
{
    if (!mModel.load(mFileName))
        return false;

    return true;
}

bool SessionEditor::save()
{
    return mModel.save(mFileName);
}

void SessionEditor::cut()
{
    mModel.undoStack().beginMacro("Cut");
    copy();
    delete_();
    mModel.undoStack().endMacro();
}

void SessionEditor::copy()
{
    auto indices = selectionModel()->selectedIndexes();
    QApplication::clipboard()->setMimeData(mModel.mimeData(indices));
}

bool SessionEditor::canPaste() const
{
    auto data = QApplication::clipboard()->mimeData();
    return mModel.canDropMimeData(data, Qt::CopyAction, -1, 0, currentIndex());
}

void SessionEditor::paste()
{
    mModel.undoStack().beginMacro("Paste");
    auto data = QApplication::clipboard()->mimeData();
    mModel.dropMimeData(data, Qt::CopyAction, -1, 0, currentIndex());
    mModel.undoStack().endMacro();
}

void SessionEditor::delete_()
{
    mModel.undoStack().beginMacro("Delete");
    auto indices = selectionModel()->selectedIndexes();
    qSort(indices.begin(), indices.end(),
        [](const auto &a, const auto &b) { return a.row() > b.row(); });
    foreach (QModelIndex index, indices)
        mModel.deleteItem(index);
    mModel.undoStack().endMacro();
}

void SessionEditor::openContextMenu(const QPoint &pos)
{
    if (!indexAt(pos).isValid())
        setCurrentIndex({ });

    auto index = selectionModel()->currentIndex();
    for (const auto &pair : {
        std::make_pair(ItemType::Group, mAddGroupAction),
        std::make_pair(ItemType::Buffer, mAddBufferAction),
        std::make_pair(ItemType::Column, mAddColumnAction),
        std::make_pair(ItemType::Texture, mAddTextureAction),
        std::make_pair(ItemType::Image, mAddImageAction),
        std::make_pair(ItemType::Sampler, mAddSamplerAction),
        std::make_pair(ItemType::Program, mAddProgramAction),
        std::make_pair(ItemType::Shader, mAddShaderAction),
        std::make_pair(ItemType::Binding, mAddBindingAction),
        std::make_pair(ItemType::Primitives, mAddPrimitivesAction),
        std::make_pair(ItemType::Attribute, mAddAttributeAction),
        std::make_pair(ItemType::Framebuffer, mAddFramebufferAction),
        std::make_pair(ItemType::Attachment, mAddAttachmentAction),
        std::make_pair(ItemType::Call, mAddCallAction),
        std::make_pair(ItemType::State, mAddStateAction),
        std::make_pair(ItemType::Script, mAddScriptAction),
    })
    pair.second->setVisible(mModel.canContainType(index, pair.first));

    mRenameAction->setEnabled(index.isValid());
    mActivateAction->setVisible(mModel.getItemType(index) == ItemType::Call);

    mContextMenu->popup(mapToGlobal(pos));
}

void SessionEditor::addItem(ItemType type)
{
    auto index = mModel.insertItem(type, currentIndex());
    setCurrentIndex(index);
    edit(index);
}

void SessionEditor::treeItemActivated(const QModelIndex &index)
{
    switch (mModel.getItemType(index)) {
        case ItemType::Group:
        case ItemType::Program:
        case ItemType::Primitives:
        case ItemType::Framebuffer:
            setExpanded(index, !isExpanded(index));
            break;

        default:
            emit itemActivated(index);
            break;
    }
}

void SessionEditor::renameCurrentItem()
{
    edit(currentIndex());
}

void SessionEditor::activateCurrentItem()
{
    emit itemActivated(currentIndex());
}
