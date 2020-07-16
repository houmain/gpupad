#include "ExpressionMatrix.h"
#include "ExpressionEditor.h"
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
            editor->setFont(parent->font());
            editor->setFrameShape(QFrame::NoFrame);
            editor->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
            editor->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
            editor->document()->setDocumentMargin(0);
            editor->setViewportMargins(3, 0, 0, 0);

            connect(editor, &ExpressionEditor::textChanged,
                this, &ExpressionItemDelegate::valueChanged);

            return editor;
        }

        void setEditorData(QWidget *editor_, const QModelIndex &index) const override
        {
            if (!mEditing) {
                auto editor = static_cast<ExpressionEditor*>(editor_);
                auto text = index.model()->data(index, Qt::EditRole).toString();
                editor->setText(text);
                editor->selectAll();
            }
        }

        void setModelData(QWidget *editor, QAbstractItemModel *model,
                          const QModelIndex &index) const override
        {
            model->setData(index,
                static_cast<ExpressionEditor*>(editor)->text());
        }

        bool helpEvent(QHelpEvent *event, QAbstractItemView *view,
            const QStyleOptionViewItem &option, const QModelIndex &index) override
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
            Q_EMIT commitData(static_cast<ExpressionEditor*>(QObject::sender()));
            mEditing = false;
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

void ExpressionMatrix::setValues(QStringList values)
{
    for (auto r = 0; r < rowCount(); ++r)
        for (auto c = 0; c < columnCount(); ++c) {
            const auto index = r * columnCount() + c;
            const auto text = (index < values.size() ?
                values.at(index) : "0");
            auto item = new QTableWidgetItem(text);
            item->setTextAlignment(Qt::AlignLeft | Qt::AlignTop);
            setItem(r, c, item);
        }
}

QStringList ExpressionMatrix::values() const
{
    auto values = QStringList();
    for (auto r = 0; r < rowCount(); ++r)
        for (auto c = 0; c < columnCount(); ++c) {
            auto value = (item(r, c) ?
                item(r, c)->text().trimmed() : "");
            values.append(!value.isEmpty() ? value : "0");
        }
    return values;
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
