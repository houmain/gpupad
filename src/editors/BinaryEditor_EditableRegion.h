#ifndef BINARYEDITOR_EDITABLEREGION_H
#define BINARYEDITOR_EDITABLEREGION_H

#include "BinaryEditor_SpinBoxDelegate.h"
#include <QTableView>
#include <QHeaderView>
#include <QMenu>
#include <QApplication>
#include <QClipboard>
#include <QMimeData>

class BinaryEditor::EditableRegion final : public QTableView
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
        if (!isVisible()) {
            actions.cut->setEnabled(false);
            actions.copy->setEnabled(false);
            actions.paste->setEnabled(false);
            actions.delete_->setEnabled(false);
            return c;
        }

        c += connect(actions.cut, &QAction::triggered, this, &EditableRegion::cut);
        c += connect(actions.copy, &QAction::triggered, this, &EditableRegion::copy);
        c += connect(actions.paste, &QAction::triggered, this, &EditableRegion::paste);
        c += connect(actions.delete_, &QAction::triggered, this, &EditableRegion::delete_);

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
        for (const QModelIndex &index : indices) {
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

        const auto rows = convertToVectors(QApplication::clipboard()->text());
        const auto begin = selectedIndexes().constFirst();
        auto row = begin.row();
        auto maxColumn = begin.column();
        for (const auto &values : rows) {
            auto column = begin.column();
            for (const auto &value : values) {
                const auto index = model()->index(row, column);
                if (index.flags() & Qt::ItemIsEditable)
                    model()->setData(index, value);
                ++column;
            }
            maxColumn = qMax(maxColumn, column);
            ++row;
        }
        Q_EMIT model()->dataChanged(begin, model()->index(row, maxColumn));
    }

    void cut()
    {
        copy();
        delete_();
    }

    void delete_()
    {
        const auto indices = selectedIndexes();
        if (indices.isEmpty())
            return;
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
    static char guessSeparator(QStringView text)
    {
        const auto commas = text.count(',');
        const auto tabs = text.count('\t');
        const auto semicolons = text.count(';');
        return (commas > tabs ? 
            (commas > semicolons ? ',' : ';') :
            (tabs > semicolons ? '\t' : ';'));
    }

    static double toNumber(QStringView value)
    {
        value = value.trimmed();
        if (value.endsWith('f') || value.endsWith('F'))
            value = value.left(value.size() - 1);
        if (value.size() > 2 && value[0] == '0' && QChar(value[1]).toLower() == 'x')
            return value.mid(2).toInt(nullptr, 16);
        return value.toDouble();
    }

    QVector<QVariantList> convertToVectors(QStringView text) const
    {
         const auto separator = guessSeparator(text);
         auto result = QVector<QVariantList>();
         for (auto line : text.split('\n')) {
             line = line.trimmed();
             if (!line.isEmpty()) {
                 auto row = QVariantList();
                 for (auto value : line.split(separator)) 
                     row.append(toNumber(value.trimmed()));
                 result.append(row);
             }
         }
         return result;
    }

    QMenu *mContextMenu{ };
};

//-------------------------------------------------------------------------

class BinaryEditor::EditableRegionDelegate final : public QItemDelegate
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
