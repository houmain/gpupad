#ifndef BINARYEDITOR_SPINBOXDELEGATE_H
#define BINARYEDITOR_SPINBOXDELEGATE_H

#include "BinaryEditor.h"
#include <QItemDelegate>
#include <QDoubleSpinBox>

namespace
{
    template <typename T>
    void setRange(QDoubleSpinBox *spinBox)
    {
        spinBox->setRange(
            static_cast<double>(std::numeric_limits<T>::lowest()),
            static_cast<double>(std::numeric_limits<T>::max()));
    }
} // namespace

class BinaryEditor::SpinBoxDelegate : public QItemDelegate
{
    Q_OBJECT
public:
    using QItemDelegate::QItemDelegate;

    QWidget *createEditor(QWidget *parent, const QStyleOptionViewItem &,
        const QModelIndex &index) const override
    {
        auto *editor = new QDoubleSpinBox(parent);
        editor->setDecimals(0);
        editor->setButtonSymbols(QAbstractSpinBox::NoButtons);
        editor->setFrame(false);

        auto dataType = static_cast<DataType>(index.data(Qt::UserRole).toInt());
        switch (dataType) {
            case DataType::Int8: setRange<int8_t>(editor); break;
            case DataType::Int16: setRange<int16_t>(editor); break;
            case DataType::Int32: setRange<int32_t>(editor); break;
            case DataType::Int64: setRange<int64_t>(editor); break;
            case DataType::Uint8: setRange<uint8_t>(editor); break;
            case DataType::Uint16: setRange<uint16_t>(editor); break;
            case DataType::Uint32: setRange<uint32_t>(editor); break;
            case DataType::Uint64: setRange<uint64_t>(editor); break;
            case DataType::Float:
                setRange<float>(editor);
                editor->setDecimals(3);
                editor->setSingleStep(0.1);
                break;
            case DataType::Double:
                setRange<double>(editor);
                editor->setDecimals(3);
                editor->setSingleStep(0.1);
                break;
        }
        return editor;
    }

    void setEditorData(QWidget *editor, const QModelIndex &index) const override
    {
        auto value = index.model()->data(index, Qt::EditRole).toDouble();
        auto *spinBox = static_cast<QDoubleSpinBox*>(editor);
        spinBox->setValue(value);
    }

    void setModelData(QWidget *editor, QAbstractItemModel *model,
        const QModelIndex &index) const override
    {
        auto *spinBox = static_cast<QDoubleSpinBox*>(editor);
        spinBox->interpretText();
        auto value = spinBox->value();
        model->setData(index, value, Qt::EditRole);
    }

    void updateEditorGeometry(QWidget *editor,
        const QStyleOptionViewItem &option, const QModelIndex &) const override
    {
        editor->setGeometry(option.rect);
    }
};

#endif // BINARYEDITOR_SPINBOXDELEGATE_H
