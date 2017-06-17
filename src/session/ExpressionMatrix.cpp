#include "ExpressionMatrix.h"
#include "ExpressionEditor.h"
#include "Singletons.h"
#include "SynchronizeLogic.h"
#include <QStyledItemDelegate>
#include <QHeaderView>
#include <QToolTip>

namespace {
    class ExpressionItemDelegate : public QStyledItemDelegate
    {
    public:
        ExpressionItemDelegate(QWidget *parent)
            : QStyledItemDelegate(parent)
        {
        }

    protected:
        QWidget *createEditor(QWidget *parent, const QStyleOptionViewItem&,
                              const QModelIndex&) const override
        {
            auto editor = new ExpressionEditor(parent);
            editor->setFrameShape(QFrame::NoFrame);
            editor->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
            editor->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);

            connect(editor,
                (&ExpressionEditor::textChanged),
                this, &ExpressionItemDelegate::valueChanged);

            return editor;
        }

        void setEditorData(QWidget *editor, const QModelIndex & index) const override
        {
            if (!mEditing)
                static_cast<ExpressionEditor*>(editor)->setText(
                    index.model()->data(index, Qt::EditRole).toString());
        }

        void setModelData(QWidget *editor, QAbstractItemModel *model,
                          const QModelIndex &index) const override
        {
            model->setData(index,
                static_cast<ExpressionEditor*>(editor)->text());
        }

        bool helpEvent(QHelpEvent *event, QAbstractItemView *view,
            const QStyleOptionViewItem& option, const QModelIndex& index)
        {
            if (event->type() == QEvent::ToolTip) {
                auto value = index.data(Qt::DisplayRole).toString();
                if (value.size() > 10) {
                    QToolTip::showText(event->globalPos(), value, view);
                    return true;
                }
            }
            return QStyledItemDelegate::helpEvent(event, view, option, index);
        }

    private:
        void valueChanged()
        {
            mEditing = true;
            emit commitData(static_cast<ExpressionEditor*>(QObject::sender()));
            mEditing = false;

            Singletons::synchronizeLogic().update();
        }

        bool mEditing{ };
    };
} // namespace

ExpressionMatrix::ExpressionMatrix(QWidget *parent) : QTableWidget(parent)
{
    setItemDelegate(new ExpressionItemDelegate(this));
}

void ExpressionMatrix::setRowCount(int rows)
{
    if (rows != rowCount()) {
        QTableWidget::setRowCount(rows);
        updateCells();
    }
}

void ExpressionMatrix::setColumnCount(int columns)
{
    if (columns != columnCount()) {
        QTableWidget::setColumnCount(columns);
        updateCells();
    }
}

void ExpressionMatrix::setFields(QStringList fields)
{
    for (auto r = 0; r < rowCount(); ++r)
        for (auto c = 0; c < columnCount(); ++c) {
            const auto index = r * columnCount() + c;
            const auto text = (index < fields.size() ?
                fields.at(index) : "0");
            auto item = new QTableWidgetItem(text);
            item->setTextAlignment(Qt::AlignLeft | Qt::AlignTop);
            setItem(r, c, item);
        }
}

QStringList ExpressionMatrix::fields() const
{
    auto fields = QStringList();
    for (auto r = 0; r < rowCount(); ++r)
        for (auto c = 0; c < columnCount(); ++c) {
            auto value = (item(r, c) ?
                item(r, c)->text().trimmed() : "");
            fields.append(!value.isEmpty() ? value : "0");
        }
    return fields;
}

void ExpressionMatrix::resizeEvent(QResizeEvent *event)
{
    QTableWidget::resizeEvent(event);
    updateCells();
}

void ExpressionMatrix::focusInEvent(QFocusEvent *event)
{
    QTableWidget::focusInEvent(event);
    if (currentIndex().isValid())
        edit(currentIndex());
}

void ExpressionMatrix::updateCells()
{
    if (rowCount() && columnCount()) {
        setMinimumHeight(rowCount() * 20);
        auto rowHeight = size().height() / rowCount();
        for (auto i = 0; i < rowCount() - 1; ++i)
            setRowHeight(i, rowHeight);
        setRowHeight(rowCount() - 1, 0);

        setMinimumWidth(columnCount() * 20);
        auto columnWidth = size().width() / columnCount();
        for (auto i = 0; i < columnCount() - 1; ++i)
            setColumnWidth(i, columnWidth);
        setColumnWidth(columnCount() - 1, 0);
    }
}
