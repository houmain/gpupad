#ifndef BINARYEDITOR_EDITABLEREGION_H
#define BINARYEDITOR_EDITABLEREGION_H

#include "BinaryEditor_SpinBoxDelegate.h"
#include <QTableView>
#include <QHeaderView>
#include <QMenu>
#include <QApplication>
#include <QClipboard>
#include <QMimeData>

class BinaryEditor::EditableRegion : public QTableView
{
    Q_OBJECT
public:
    EditableRegion(int columnWidth, int rowHeight, QWidget *parent)
        : QTableView(parent)
        , mContextMenu(new QMenu(this))
    {
        setFrameShape(QFrame::NoFrame);
        setAutoScroll(false);
        setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
        setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
        setWordWrap(false);
        setTextElideMode(Qt::ElideNone);
        setAlternatingRowColors(true);
        setSelectionMode(QAbstractItemView::ContiguousSelection);
        setContextMenuPolicy(Qt::CustomContextMenu);

        horizontalHeader()->setDefaultSectionSize(columnWidth);
        horizontalHeader()->setSectionResizeMode(QHeaderView::Fixed);
        horizontalHeader()->setHighlightSections(false);

        verticalHeader()->setDefaultSectionSize(rowHeight);
        verticalHeader()->setSectionResizeMode(QHeaderView::Fixed);
        verticalHeader()->setHighlightSections(false);

        setItemDelegate(new SpinBoxDelegate(this));

        connect(this, &EditableRegion::customContextMenuRequested,
            this, &EditableRegion::openContextMenu);
    }

    QList<QMetaObject::Connection> connectEditActions(const EditActions &actions)
    {
        auto c = QList<QMetaObject::Connection>();
        //c += connect(actions.undo, &QAction::triggered, this, &EditableRegion::undo);
        //c += connect(actions.redo, &QAction::triggered, this, &EditableRegion::redo);
        c += connect(actions.cut, &QAction::triggered, this, &EditableRegion::cut);
        c += connect(actions.copy, &QAction::triggered, this, &EditableRegion::copy);
        c += connect(actions.paste, &QAction::triggered, this, &EditableRegion::paste);
        c += connect(actions.delete_, &QAction::triggered, this, &EditableRegion::delete_);
        //c += connect(actions.selectAll, &QAction::triggered, this, &EditableRegion::selectAll);

        actions.cut->setEnabled(true);
        actions.copy->setEnabled(true);
        actions.paste->setEnabled(canPaste());
        actions.delete_->setEnabled(true);

        c += connect(QApplication::clipboard(), &QClipboard::changed,
            [this, actions]() { actions.paste->setEnabled(canPaste()); });

        mContextMenu->clear();
        mContextMenu->addAction(actions.undo);
        mContextMenu->addAction(actions.redo);
        mContextMenu->addSeparator();
        mContextMenu->addAction(actions.cut);
        mContextMenu->addAction(actions.copy);
        mContextMenu->addAction(actions.paste);
        mContextMenu->addAction(actions.delete_);
        mContextMenu->addSeparator();
        return c;
    }

    void openContextMenu(const QPoint &pos)
    {
        mContextMenu->popup(mapToGlobal(pos));
    }

    void copy()
    {
        auto str = QString();
        auto prevRow = -1;
        auto newLine = true;
        const auto indices = selectedIndexes();
        for (QModelIndex index : indices) {
            auto row = index.row();
            if (prevRow != -1 && row != prevRow) {
                str += "\n";
                newLine = true;
            }
            if (!newLine)
                str += "\t";
            str += index.data().toString();
            newLine = false;
            prevRow = row;
        }
        QApplication::clipboard()->setText(str);
    }

    bool canPaste() const
    {
        if (auto mimeData = QApplication::clipboard()->mimeData())
            return mimeData->hasText();
        return false;
    }

    void paste()
    {
        if (selectedIndexes().isEmpty())
            return;

        const auto text = QApplication::clipboard()->text();
        const auto separator = guessSeparator(text);
        const auto begin = selectedIndexes().constFirst();
        auto r = begin.row();
        auto c = begin.column();
        const auto rows = text.split('\n');
        for (const auto &row : rows) {
            if (row.trimmed().isEmpty())
                continue;
            c = begin.column();
            const auto values = row.split(separator);
            for (const auto &value : values) {
                const auto index = model()->index(r, c);
                if (index.flags() & Qt::ItemIsEditable)
                    model()->setData(index, toNumber(value));
                ++c;
            }
            ++r;
        }
        Q_EMIT model()->dataChanged(begin, model()->index(r, c));
    }

    double toNumber(QString value)
    {
        value = value.trimmed();
        if (value.endsWith('f') || value.endsWith('F'))
            value.resize(value.size() - 1);
        return value.toDouble();
    }

    void cut()
    {
        copy();
        delete_();
    }

    void delete_()
    {
        const auto indices = selectedIndexes();
        auto minIndex = indices.first();
        auto maxIndex = indices.first();
        for (const QModelIndex &index : indices)
            if (index.flags() & Qt::ItemIsEditable) {
                model()->setData(index, 0.0);
                minIndex = model()->index(
                    std::min(index.row(), minIndex.row()),
                    std::min(index.column(), minIndex.column()));
                maxIndex = model()->index(
                    std::max(index.row(), maxIndex.row()),
                    std::max(index.column(), maxIndex.column()));
            }
        Q_EMIT model()->dataChanged(minIndex, maxIndex);
    }

private:
    static char guessSeparator(const QString &text)
    {
        const auto commas = text.count(',');
        const auto tabs = text.count('\t');
        const auto semicolons = text.count(';');
        if (commas > tabs)
            return (commas > semicolons ? ',' : ';');
        else
            return (tabs > semicolons ? '\t' : ';');
    }

    QMenu *mContextMenu{ };
};

//-------------------------------------------------------------------------

class BinaryEditor::EditableRegionDelegate : public QItemDelegate
{
    Q_OBJECT
public:
    EditableRegionDelegate(QWidget *editor, QWidget *parent)
        : QItemDelegate(parent)
        , mEditableRegion(editor)
    {
    }

    QWidget *createEditor(QWidget *parent, const QStyleOptionViewItem &,
        const QModelIndex &) const override
    {
        mEditableRegion->setParent(parent);
        return mEditableRegion;
    }

    void destroyEditor(QWidget *editor, const QModelIndex &) const override
    {
        editor->setParent(static_cast<QWidget*>(parent()));
    }

    void updateEditorGeometry(QWidget *editor,
        const QStyleOptionViewItem &option,
        const QModelIndex &) const override
    {
        editor->setGeometry(option.rect);
    }

private:
    QWidget *mEditableRegion{ };
};

#endif // BINARYEDITOR_EDITABLEREGION_H
