#include "SessionEditor.h"
#include "EditActions.h"
#include "FileDialog.h"
#include "SessionModel.h"
#include "Singletons.h"
#include "properties/PropertiesEditor.h"
#include <QApplication>
#include <QClipboard>
#include <QMenu>
#include <QMimeData>
#include <QDropEvent>

SessionEditor::SessionEditor(QWidget *parent)
    : QTreeView(parent)
    , mModel(Singletons::sessionModel())
    , mContextMenu(new QMenu(this))
    , mComponentContextMenu(new QMenu(this))
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
    setRootIsDecorated(false);

    connect(this, &QTreeView::activated, this,
        &SessionEditor::handleItemActivated);
    connect(this, &QTreeView::customContextMenuRequested, this,
        &SessionEditor::openContextMenu);
    connect(&mModel, &SessionModel::rowsInserted, this,
        [this](const QModelIndex &parent, int first, int last) {
            hideComponentContent(parent);
            for (auto row = first; row <= last; ++row)
                hideComponentContent(mModel.index(row, 0, parent));
        });
    connect(&mModel, &SessionModel::dataChanged, this,
        [this](const QModelIndex &topLeft, const QModelIndex &) {
            if (topLeft.column() == SessionModel::ComponentInterface)
                hideComponentContent(topLeft.parent());
        });

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
            tr("Add ") + splitPascalCase(typeName), this);
        connect(action, &QAction::triggered, [this, type]() { addItem(type); });
    };
    addAction(mAddGroupAction, Item::Type::Group);
    addAction(mAddScriptAction, Item::Type::Script);
    addAction(mAddProgramAction, Item::Type::Program);
    addAction(mAddBufferAction, Item::Type::Buffer);
    addAction(mAddTextureAction, Item::Type::Texture);
    addAction(mAddCallAction, Item::Type::Call);
    addAction(mAddAttachmentAction, Item::Type::Attachment);
    addAction(mAddShaderAction, Item::Type::Shader);
    addAction(mAddShaderTableAction, Item::Type::ShaderTable);
    addAction(mAddRecordAction, Item::Type::Record);
    addAction(mAddBindingAction, Item::Type::Binding);
    addAction(mAddBlockAction, Item::Type::Block);
    addAction(mAddFieldAction, Item::Type::Field);
    addAction(mAddTargetAction, Item::Type::Target);
    addAction(mAddAttributeAction, Item::Type::Attribute);
    addAction(mAddStreamAction, Item::Type::Stream);
    addAction(mAddGeometrySetAction, Item::Type::GeometrySet);
    addAction(mAddGeometryAction, Item::Type::Geometry);
    addAction(mAddAccelerationStructureAction,
        Item::Type::AccelerationStructure);
    addAction(mAddInstanceAction, Item::Type::Instance);

    mAddComponentAction = new QAction(mModel.getTypeIcon(Item::Type::Component),
        tr("Add &Component..."), this);
    connect(mAddComponentAction, &QAction::triggered, this,
        &SessionEditor::addComponent);

    addItemActions(mContextMenu);

    // WORKAROUND: checkbox border is too dark in dark theme
    QPalette p = palette();
    p.setColor(QPalette::Window, 0xCCCCCC);
    setPalette(p);

    selectSession();
}

void SessionEditor::addItemActions(QMenu *menu)
{
    connect(menu, &QMenu::aboutToShow, this, &SessionEditor::updateItemActions,
        Qt::UniqueConnection);
    menu->addAction(mAddBlockAction);
    menu->addAction(mAddFieldAction);
    menu->addAction(mAddAttributeAction);
    menu->addAction(mAddAttachmentAction);
    menu->addAction(mAddShaderAction);
    menu->addAction(mAddRecordAction);
    menu->addAction(mAddGeometryAction);
    menu->addAction(mAddInstanceAction);
    menu->addSeparator();
    menu->addAction(mAddGroupAction);
    menu->addAction(mAddComponentAction);
    menu->addAction(mAddBufferAction);
    menu->addAction(mAddStreamAction);
    menu->addAction(mAddTextureAction);
    menu->addAction(mAddTargetAction);
    menu->addAction(mAddBindingAction);
    menu->addAction(mAddProgramAction);
    menu->addAction(mAddShaderTableAction);
    menu->addAction(mAddCallAction);
    menu->addAction(mAddScriptAction);
    menu->addAction(mAddGeometrySetAction);
    menu->addAction(mAddAccelerationStructureAction);
}

void SessionEditor::updateItemActions()
{
    auto index = selectionModel()->currentIndex();
    struct TypeAction
    {
        Item::Type type;
        QAction *action;
    };
    for (const auto [type, action] : {
             TypeAction{ Item::Type::Block, mAddBlockAction },
             TypeAction{ Item::Type::Field, mAddFieldAction },
             TypeAction{ Item::Type::Shader, mAddShaderAction },
             TypeAction{ Item::Type::Record, mAddRecordAction },
             TypeAction{ Item::Type::Attribute, mAddAttributeAction },
             TypeAction{ Item::Type::Attachment, mAddAttachmentAction },
             TypeAction{ Item::Type::Instance, mAddInstanceAction },
             TypeAction{ Item::Type::Geometry, mAddGeometryAction },
             TypeAction{ Item::Type::GeometrySet, mAddGeometrySetAction },
         })
        action->setVisible(mModel.canContainType(index, type)
            || mModel.canContainType(index.parent(), type));
}

QList<QMetaObject::Connection> SessionEditor::connectEditActions(
    const EditActions &actions)
{
    auto c = QList<QMetaObject::Connection>();
    actions.windowFileName->setText(fileName());
    actions.windowFileName->setEnabled(isModified());
    c += connect(this, &SessionEditor::fileNameChanged, actions.windowFileName,
        &QAction::setText);
    c += connect(this, &SessionEditor::modificationChanged,
        actions.windowFileName, &QAction::setEnabled);

    c += mModel.connectUndoActions(actions.undo, actions.redo);
    c += connect(&mModel, &SessionModel::undoStackCleanChanged,
        [this](bool isClean) { Q_EMIT this->modificationChanged(!isClean); });

    // do not enable copy/paste... actions when a property editor is focused
    if (qApp->focusWidget() != this)
        return c;

    c += connect(actions.cut, &QAction::triggered, this, &SessionEditor::cut);
    c += connect(actions.copy, &QAction::triggered, this, &SessionEditor::copy);
    c += connect(actions.paste, &QAction::triggered, this,
        &SessionEditor::paste);
    c += connect(actions.delete_, &QAction::triggered, this,
        &SessionEditor::delete_);
    c += connect(actions.rename, &QAction::triggered, this,
        &SessionEditor::renameCurrentItem);

    const auto updateEditActions = [this, actions]() {
        const auto isItemSelected = hasFocus() && currentIndex().isValid()
            && mModel.getItemType(currentIndex()) != Item::Type::Session;
        const auto isComponentContent =
            mModel.isComponentContent(currentIndex());
        actions.cut->setEnabled(isItemSelected && !isComponentContent);
        actions.copy->setEnabled(isItemSelected);
        actions.delete_->setEnabled(isItemSelected && !isComponentContent);
        actions.rename->setEnabled(isItemSelected);
        actions.paste->setEnabled(hasFocus() && canPaste()
            && !isComponentContent);
    };
    c += connect(selectionModel(), &QItemSelectionModel::currentChanged,
        updateEditActions);
    c += connect(this, &SessionEditor::focusChanged, updateEditActions);
    c += connect(QApplication::clipboard(), &QClipboard::changed,
        updateEditActions);
    updateEditActions();

    const auto addEditActions = [&actions](QMenu *menu) {
        const auto pos = (menu->actions().isEmpty()
                ? nullptr
                : menu->actions().constFirst());
        if (pos == actions.undo)
            return;
        menu->insertAction(pos, actions.undo);
        menu->insertAction(pos, actions.redo);
        menu->insertSeparator(pos);
        menu->insertActions(pos,
            { actions.cut, actions.copy, actions.paste, actions.delete_ });
        if (pos)
            menu->insertSeparator(pos);
    };
    addEditActions(mContextMenu);
    addEditActions(mComponentContextMenu);
    return c;
}

void SessionEditor::mouseReleaseEvent(QMouseEvent *event)
{
    QTreeView::mouseReleaseEvent(event);

    // keep current selected
    if (selectionModel()->selection().isEmpty() && currentIndex().isValid()) {
        selectionModel()->select(currentIndex(), QItemSelectionModel::Select);

        // ensure selected item is still visible
        scrollTo(currentIndex());
    }
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
    for (const QModelIndex &index : selection.indexes())
        if (index.parent() != parent)
            invalid.select(index, index);

    if (!invalid.empty()) {
        selectionModel()->select(invalid, QItemSelectionModel::Deselect);
        QTreeView::selectionChanged(selectionModel()->selection(), deselected);
    } else {
        QTreeView::selectionChanged(selected, deselected);
    }
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

void SessionEditor::dropEvent(QDropEvent *event)
{
    QTreeView::dropEvent(event);

    // ensure Session stays expanded
    setExpanded(mModel.sessionItemIndex(), true);
}

bool SessionEditor::isModified() const
{
    return !mModel.isUndoStackClean();
}

void SessionEditor::clearUndo()
{
    mModel.clearUndoStack();
}

bool SessionEditor::clear()
{
    mModel.clear();
    if (!FileDialog::isUntitled(mFileName))
        setFileName({ });
    selectSession();
    return true;
}

void SessionEditor::setFileName(QString fileName)
{
    if (fileName.isEmpty())
        fileName =
            FileDialog::generateNextUntitledFileName(tr("Untitled Session"));

    if (mFileName != fileName) {
        mFileName = fileName;
        Q_EMIT fileNameChanged(mFileName);
    }
}

bool SessionEditor::load()
{
    setAnimated(false);
    if (!mModel.load(mFileName))
        return false;
    selectSession();
    setAnimated(true);
    return true;
}

bool SessionEditor::save()
{
    return mModel.save(mFileName);
}

void SessionEditor::cut()
{
    mModel.beginUndoMacro("Cut");
    copy();
    delete_();
    mModel.endUndoMacro();
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
    const auto drop = [&](auto row, auto column, auto parent) {
        auto mimeData = QApplication::clipboard()->mimeData();
        if (!mModel.canDropMimeData(mimeData, Qt::CopyAction, row, column,
                parent))
            return false;

        const auto dropped =
            mModel.dropMimeData(mimeData, Qt::CopyAction, row, column, parent);
        if (dropped) {
            if (row < 0)
                row = mModel.rowCount(parent) - 1;
            setCurrentItem(mModel.getItemId(mModel.index(row, column, parent)));
        }
        return dropped;
    };

    // try to drop as child first
    if (drop(-1, 0, currentIndex()))
        return;

    // drop as sibling
    if (currentIndex().isValid())
        drop(currentIndex().row() + 1, 0, currentIndex().parent());
}

void SessionEditor::delete_()
{
    mModel.beginUndoMacro("Delete");
    auto indices = selectionModel()->selectedIndexes();
    std::sort(indices.begin(), indices.end(),
        [](const auto &a, const auto &b) { return a.row() > b.row(); });
    for (const auto &index : std::as_const(indices))
        mModel.deleteItem(index);
    mModel.endUndoMacro();
}

void SessionEditor::setCurrentItem(ItemId itemId)
{
    if (auto item = mModel.findItem(itemId)) {
        auto index = mModel.getIndex(item, SessionModel::Name);
        selectionModel()->clear();
        setCurrentIndex(index);
        scrollTo(index);
    }
}

void SessionEditor::selectSession()
{
    const auto index = mModel.sessionItemIndex();
    setCurrentIndex(index);
    expand(index);
}

void SessionEditor::openContextMenu(const QPoint &pos)
{
    if (!indexAt(pos).isValid())
        selectSession();

    updateItemActions();

    auto *menu = (mModel.isComponentOrContent(currentIndex())
            ? mComponentContextMenu
            : mContextMenu);
    menu->popup(mapToGlobal(pos));
}

void SessionEditor::addItem(Item::Type type)
{
    if (mModel.isComponentOrContent(currentIndex()))
        return;

    mModel.beginUndoMacro("Add");

    const auto index = mModel.insertItem(type, currentIndex(), -1);

    if (type == Item::Type::Buffer) {
        const auto block = mModel.insertItem(Item::Type::Block, index);
        mModel.insertItem(Item::Type::Field, block);
        setExpanded(block, true);
    } else if (type == Item::Type::Target)
        mModel.insertItem(Item::Type::Attachment, index);
    else if (type == Item::Type::Stream)
        mModel.insertItem(Item::Type::Attribute, index);
    else if (type == Item::Type::Program)
        mModel.insertItem(Item::Type::Shader, index);
    else if (type == Item::Type::ShaderTable)
        mModel.insertItem(Item::Type::Record, index);
    else if (type == Item::Type::AccelerationStructure) {
        const auto instance = mModel.insertItem(Item::Type::Instance, index);
        mModel.insertItem(Item::Type::Geometry, instance);
        setExpanded(instance, true);
    } else if (type == Item::Type::GeometrySet)
        mModel.insertItem(Item::Type::Geometry, index);

    mModel.endUndoMacro();

    setCurrentIndex(index);
    setExpanded(index, true);
    Q_EMIT itemAdded(index);
}

void SessionEditor::addComponent()
{
    if (mModel.isComponentOrContent(currentIndex()))
        return;

    auto &dialog = Singletons::fileDialog();
    if (!dialog.exec(FileDialog::SessionExtensions))
        return;

    const auto index =
        mModel.insertComponent(dialog.fileName(), currentIndex());
    if (!index.isValid())
        return;

    setCurrentIndex(index);
    setExpanded(index, true);
    Q_EMIT itemAdded(index);
}

void SessionEditor::hideComponentContent(const QModelIndex &index)
{
    if (!mModel.item<Component>(index)) {
        if (!mModel.item<Component>(index.parent())
            || mModel.getItem(index).componentInterface
                == Item::Interface::Internal)
            return;
        for (auto row = 0; row < mModel.rowCount(index); ++row)
            setRowHidden(row, index, true);
        return;
    }

    for (auto row = 0; row < mModel.rowCount(index); ++row) {
        const auto child = mModel.index(row, 0, index);
        const auto interface = mModel.getItem(child).componentInterface;
        setRowHidden(row, index, interface == Item::Interface::Internal);
        if (interface != Item::Interface::Internal) {
            setExpanded(child, false);
            for (auto i = 0; i < mModel.rowCount(child); ++i)
                setRowHidden(i, child, true);
        }
    }
}

void SessionEditor::activateFirstTextureItem()
{
    auto index = QModelIndex{ };
    mModel.forEachItem([&](const Item &item) {
        if (item.type == Item::Type::Texture && !index.isValid())
            index = mModel.getIndex(&item);
    });
    if (index.isValid())
        Q_EMIT itemActivated(index);
}

void SessionEditor::handleItemActivated(const QModelIndex &index)
{
    if (mModel.item<Group>(index) || mModel.item<Component>(index)) {
        setExpanded(index, !isExpanded(index));
    } else {
        Q_EMIT itemActivated(index);
    }
}

void SessionEditor::renameCurrentItem()
{
    edit(currentIndex());
}
