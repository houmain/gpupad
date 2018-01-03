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
    setMinimumWidth(150);

    connect(&mModel.undoStack(), &QUndoStack::cleanChanged,
        [this](bool clean) { emit modificationChanged(!clean); });
    connect(this, &QTreeView::activated,
        this, &SessionEditor::treeItemActivated);
    connect(this, &QTreeView::customContextMenuRequested,
        this, &SessionEditor::openContextMenu);

    auto usedLetters = QSet<QChar>();
    auto addAction = [&](auto &action, auto type) {
        auto typeName = mModel.getTypeName(type);

        // automatically insert & before first unused letter
        for (auto i = 0; i < typeName.size(); i++) {
            const auto c = typeName[i].toLower();
            if (!usedLetters.contains(c)) {
                usedLetters.insert(c);
                typeName = typeName.insert(i, '&');
                break;
            }
        }
        action = new QAction(mModel.getTypeIcon(type),
            tr("Add ") + typeName, this);
        connect(action, &QAction::triggered,
            [this, type]() { addItem(type); });
    };
    addAction(mAddGroupAction, Item::Type::Group);
    addAction(mAddScriptAction, Item::Type::Script);
    addAction(mAddProgramAction, Item::Type::Program);
    addAction(mAddBufferAction, Item::Type::Buffer);
    addAction(mAddTextureAction, Item::Type::Texture);
    addAction(mAddCallAction, Item::Type::Call);
    addAction(mAddAttachmentAction, Item::Type::Attachment);
    addAction(mAddShaderAction, Item::Type::Shader);
    addAction(mAddBindingAction, Item::Type::Binding);
    addAction(mAddImageAction, Item::Type::Image);
    addAction(mAddColumnAction, Item::Type::Column);
    addAction(mAddTargetAction, Item::Type::Target);
    addAction(mAddAttributeAction, Item::Type::Attribute);
    addAction(mAddStreamAction, Item::Type::Stream);

    addItemActions(mContextMenu);
}

void SessionEditor::addItemActions(QMenu* menu)
{
    menu->addAction(mAddColumnAction);
    menu->addAction(mAddAttributeAction);
    menu->addAction(mAddImageAction);
    menu->addAction(mAddAttachmentAction);
    menu->addAction(mAddShaderAction);
    menu->addSeparator();
    menu->addAction(mAddGroupAction);
    menu->addAction(mAddBufferAction);
    menu->addAction(mAddStreamAction);
    menu->addAction(mAddTextureAction);
    menu->addAction(mAddTargetAction);
    menu->addAction(mAddBindingAction);
    menu->addAction(mAddProgramAction);
    menu->addAction(mAddCallAction);
    menu->addAction(mAddScriptAction);
}

void SessionEditor::updateItemActions()
{
    auto index = selectionModel()->currentIndex();
    for (const auto &pair : {
            std::make_pair(Item::Type::Column, mAddColumnAction),
            std::make_pair(Item::Type::Image, mAddImageAction),
            std::make_pair(Item::Type::Shader, mAddShaderAction),
            std::make_pair(Item::Type::Attribute, mAddAttributeAction),
            std::make_pair(Item::Type::Attachment, mAddAttachmentAction),
        })
        pair.second->setVisible(
            mModel.canContainType(index, pair.first) ||
            mModel.canContainType(index.parent(), pair.first));
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
    c += connect(actions.rename, &QAction::triggered,
        this, &SessionEditor::renameCurrentItem);

    auto updateEditActions = [this, actions]() {
        actions.cut->setEnabled(hasFocus() && currentIndex().isValid());
        actions.copy->setEnabled(hasFocus() && currentIndex().isValid());
        actions.delete_->setEnabled(hasFocus() && currentIndex().isValid());
        actions.paste->setEnabled(hasFocus() && canPaste());
        actions.rename->setEnabled(hasFocus() && currentIndex().isValid());
    };
    c += connect(selectionModel(), &QItemSelectionModel::currentChanged,
        updateEditActions);
    c += connect(this, &SessionEditor::focusChanged, updateEditActions);
    c += connect(QApplication::clipboard(), &QClipboard::changed,
        updateEditActions);

    auto pos = mContextMenu->actions().constFirst();
    if (pos != actions.undo) {
        mContextMenu->insertAction(pos, actions.undo);
        mContextMenu->insertAction(pos, actions.redo);
        mContextMenu->insertSeparator(pos);
        mContextMenu->insertActions(pos, {
            actions.cut, actions.copy, actions.paste, actions.delete_ });
        mContextMenu->insertSeparator(pos);
    }
    return c;
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

bool SessionEditor::isModified() const
{
    return !mModel.undoStack().isClean();
}

bool SessionEditor::clear()
{
    mModel.clear();
    if (!FileDialog::isUntitled(mFileName))
        setFileName({ });
    return true;
}

void SessionEditor::setFileName(QString fileName)
{
    if (fileName.isEmpty())
        fileName = FileDialog::generateNextUntitledFileName(
            tr("Untitled Session"), true);

    if (mFileName != fileName) {
        mFileName = fileName;
        emit fileNameChanged(mFileName);
    }
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
    // check if data can be dropped as sibling or child
    if (mModel.canDropMimeData(data, Qt::CopyAction,
            currentIndex().row() + 1, 0, currentIndex().parent()))
        return true;
    if (mModel.canDropMimeData(data, Qt::CopyAction, -1, 0, currentIndex()))
        return true;

    return false;
}

void SessionEditor::paste()
{
    mModel.undoStack().beginMacro("Paste");
    auto data = QApplication::clipboard()->mimeData();
    // try to drop as sibling first
    if (currentIndex().isValid() &&
        mModel.canDropMimeData(data, Qt::CopyAction,
            currentIndex().row() + 1, 0, currentIndex().parent())) {
        mModel.dropMimeData(data, Qt::CopyAction,
            currentIndex().row() + 1, 0, currentIndex().parent());
    }
    else {
        mModel.dropMimeData(data, Qt::CopyAction, -1, 0, currentIndex());
    }
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

void SessionEditor::setCurrentItem(ItemId itemId)
{
    if (auto item = mModel.findItem(itemId)) {
        auto index = mModel.getIndex(item);
        setCurrentIndex(index);
        scrollTo(index);
    }
}

void SessionEditor::openContextMenu(const QPoint &pos)
{
    if (!indexAt(pos).isValid())
        setCurrentIndex({ });

    updateItemActions();

    mContextMenu->popup(mapToGlobal(pos));
}

void SessionEditor::addItem(Item::Type type)
{
    mModel.undoStack().beginMacro("Add");

    auto addingToGroup = (currentIndex().isValid() &&
        mModel.getItemType(currentIndex()) == Item::Type::Group);
    auto index = mModel.insertItem(type, currentIndex(), addingToGroup ? 0 : -1);

    if (type == Item::Type::Buffer)
        mModel.insertItem(Item::Type::Column, index);
    else if (type == Item::Type::Target)
        mModel.insertItem(Item::Type::Attachment, index);
    else if (type == Item::Type::Stream)
        mModel.insertItem(Item::Type::Attribute, index);
    else if (type == Item::Type::Program)
        mModel.insertItem(Item::Type::Shader, index);

    mModel.undoStack().endMacro();

    setCurrentIndex(index);
    setExpanded(index, true);
    emit itemAdded(index);
}

void SessionEditor::activateFirstItem()
{
    treeItemActivated(model()->index(0, 0));
}

void SessionEditor::treeItemActivated(const QModelIndex &index)
{
    auto handled = false;
    emit itemActivated(index, &handled);
    if (!handled)
        setExpanded(index, !isExpanded(index));
}

void SessionEditor::renameCurrentItem()
{
    edit(currentIndex());
}
