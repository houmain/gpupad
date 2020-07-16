#include "SessionEditor.h"
#include "Singletons.h"
#include "SessionModel.h"
#include "EditActions.h"
#include "FileDialog.h"
#include <QApplication>
#include <QClipboard>
#include <QMimeData>
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
        [this](bool clean) { Q_EMIT modificationChanged(!clean); });
    connect(this, &QTreeView::activated,
        this, &SessionEditor::handleItemActivated);
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
    addAction(mAddColumnAction, Item::Type::Column);
    addAction(mAddTargetAction, Item::Type::Target);
    addAction(mAddAttributeAction, Item::Type::Attribute);
    addAction(mAddStreamAction, Item::Type::Stream);

    addItemActions(mContextMenu);

    // WORKAROUND: checkbox border is too dark in dark theme
    QPalette p = palette();
    p.setColor(QPalette::Window, "#CCC");
    setPalette(p);
}

void SessionEditor::addItemActions(QMenu* menu)
{
    menu->addAction(mAddColumnAction);
    menu->addAction(mAddAttributeAction);
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
            std::make_pair(Item::Type::Shader, mAddShaderAction),
            std::make_pair(Item::Type::Attribute, mAddAttributeAction),
            std::make_pair(Item::Type::Attachment, mAddAttachmentAction),
        })
        pair.second->setVisible(
            mModel.canContainType(index, pair.first) ||
            mModel.canContainType(index.parent(), pair.first));
}

QList<QMetaObject::Connection> SessionEditor::connectEditActions(
    const EditActions &actions)
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
    if (qApp->focusWidget() != this)
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

void SessionEditor::wheelEvent(QWheelEvent *event)
{
    setFocus();
    QTreeView::wheelEvent(event);
}

void SessionEditor::selectionChanged(const QItemSelection &selected,
                                     const QItemSelection &deselected)
{
    // do not allow to select item with different parents
    const auto parent = currentIndex().parent();
    const auto selection = selectionModel()->selection();
    auto invalid = QItemSelection();
    for (QModelIndex index : selection.indexes())
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
    Q_EMIT focusChanged(true);
}

void SessionEditor::focusOutEvent(QFocusEvent *event)
{
    QTreeView::focusOutEvent(event);
    Q_EMIT focusChanged(false);
}

bool SessionEditor::isModified() const
{
    return !mModel.undoStack().isClean();
}

void SessionEditor::clearUndo()
{
    mModel.undoStack().clear();
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
            tr("Untitled Session"));

    if (mFileName != fileName) {
        mFileName = fileName;
        Q_EMIT fileNameChanged(mFileName);
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
    if (auto mimeData = QApplication::clipboard()->mimeData())
        return mimeData->hasText();
    return false;
}

void SessionEditor::paste()
{
    auto drop = [&](auto row, auto column, auto parent) {
        auto mimeData = QApplication::clipboard()->mimeData();
        if (!mModel.canDropMimeData(mimeData, Qt::CopyAction, row, column, parent))
            return false;
        mModel.undoStack().beginMacro("Paste");
        mModel.dropMimeData(mimeData, Qt::CopyAction, row, column, parent);
        mModel.undoStack().endMacro();
        return true;
    };

    // try to drop as sibling first
    if (currentIndex().isValid() &&
        drop(currentIndex().row() + 1, 0, currentIndex().parent()))
        return;

    drop(-1, 0, currentIndex());
}

void SessionEditor::delete_()
{
    mModel.undoStack().beginMacro("Delete");
    auto indices = selectionModel()->selectedIndexes();
    std::sort(indices.begin(), indices.end(),
        [](const auto &a, const auto &b) { return a.row() > b.row(); });
    for (QModelIndex index : indices)
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
    Q_EMIT itemAdded(index);
}

void SessionEditor::activateFirstItem()
{
    Q_EMIT itemActivated(model()->index(0, 0));
}

void SessionEditor::handleItemActivated(const QModelIndex &index)
{
    if (mModel.getItemType(index) == Item::Type::Group) {
        setExpanded(index, !isExpanded(index));
    }
    else {
        Q_EMIT itemActivated(index);
   }
}

void SessionEditor::renameCurrentItem()
{
    edit(currentIndex());
}
