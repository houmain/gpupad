#pragma once

#include "BinaryEditor.h"
#include "session/ExpressionLineEdit.h"
#include <QItemDelegate>
#include <QSpinBox>

namespace {
    template <typename T>
    void setRange(QDoubleSpinBox *spinBox)
    {
        spinBox->setRange(std::numeric_limits<T>::lowest(),
            std::numeric_limits<T>::max());
    }
} // namespace

class BinaryEditor::SpinBoxDelegate final : public QItemDelegate
{
    Q_OBJECT
public:
    using QItemDelegate::QItemDelegate;

    QWidget *createEditor(QWidget *parent, const QStyleOptionViewItem &,
        const QModelIndex &index) const override
    {
        const auto dataType =
            static_cast<DataType>(index.data(Qt::UserRole).toInt());

        if (dataType == DataType::Float || dataType == DataType::Double) {
            auto *editor = new ExpressionLineEdit(parent);
            editor->setFrame(false);
            editor->setDecimal(true);

            connect(editor, &ExpressionLineEdit::textChanged,
                [this, editor, index]() {
                    setModelData(editor,
                        const_cast<QAbstractItemModel *>(index.model()), index);
                });
            return editor;
        }

        auto *editor = new QDoubleSpinBox(parent);
        editor->setButtonSymbols(QAbstractSpinBox::NoButtons);
        editor->setFrame(false);
        editor->setDecimals(0);
        switch (dataType) {
        case DataType::Int8:   setRange<int8_t>(editor); break;
        case DataType::Int16:  setRange<int16_t>(editor); break;
        case DataType::Int32:  setRange<int32_t>(editor); break;
        //case DataType::Int64: setRange<int64_t>(editor); break;
        case DataType::Uint8:  setRange<uint8_t>(editor); break;
        case DataType::Uint16: setRange<uint16_t>(editor); break;
        case DataType::Uint32: setRange<uint32_t>(editor); break;
        //case DataType::UInt64: setRange<uint64_t>(editor); break;
        default:               break;
        }

        connect(editor, qOverload<double>(&QDoubleSpinBox::valueChanged),
            [this, editor, index]() {
                setModelData(editor,
                    const_cast<QAbstractItemModel *>(index.model()), index);
            });
        return editor;
    }

    void setEditorData(QWidget *editor, const QModelIndex &index) const override
    {
        if (auto *spinBox = qobject_cast<QDoubleSpinBox *>(editor)) {
            auto value = index.model()->data(index, Qt::EditRole).toInt();
            spinBox->setValue(value);
        } else if (
            auto *lineEdit = qobject_cast<ExpressionLineEdit *>(editor)) {
            auto value = index.model()->data(index, Qt::EditRole).toString();
            lineEdit->setText(value);
        }
    }

    void setModelData(QWidget *editor, QAbstractItemModel *model,
        const QModelIndex &index) const override
    {
        if (auto *spinBox = qobject_cast<QDoubleSpinBox *>(editor)) {
            spinBox->interpretText();
            auto value = spinBox->value();
            if (model->data(index, Qt::EditRole) != value) {
                model->setData(index, value, Qt::EditRole);
                Q_EMIT model->dataChanged(index, index);
            }
        } else if (
            auto *lineEdit = qobject_cast<ExpressionLineEdit *>(editor)) {
            auto ok = true;
            const auto value = lineEdit->text().toDouble(&ok);
            const auto current = model->data(index, Qt::EditRole);
            if (ok
                && (current.isNull()
                    || !lineEdit->hasValue(current.toDouble()))) {
                model->setData(index, value, Qt::EditRole);
                Q_EMIT model->dataChanged(index, index);
            }
        }
    }

    void updateEditorGeometry(QWidget *editor,
        const QStyleOptionViewItem &option, const QModelIndex &) const override
    {
        editor->setGeometry(option.rect);
    }
};
