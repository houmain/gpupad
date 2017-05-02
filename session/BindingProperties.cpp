#include "BindingProperties.h"
#include "SessionModel.h"
#include "ExpressionEditor.h"
#include "Singletons.h"
#include "SynchronizeLogic.h"
#include "SessionProperties.h"
#include <QTableWidget>
#include <QStyledItemDelegate>
#include <QHeaderView>
#include <QToolTip>

namespace {
    int typeColumns(Binding::Type type)
    {
        switch (type) {
            case Binding::Matrix2x2:
            case Binding::Matrix3x2:
            case Binding::Matrix4x2: return 2;
            case Binding::Matrix2x3:
            case Binding::Matrix3x3:
            case Binding::Matrix4x3: return 3;
            case Binding::Matrix2x4:
            case Binding::Matrix3x4:
            case Binding::Matrix4x4: return 4;
            default: return 1;
        }
    }

    int typeRows(Binding::Type type)
    {
        switch (type) {
            case Binding::Matrix2x2:
            case Binding::Matrix2x3:
            case Binding::Matrix2x4:
            case Binding::Vector2: return 2;
            case Binding::Matrix3x2:
            case Binding::Matrix3x3:
            case Binding::Matrix3x4:
            case Binding::Vector3: return 3;
            case Binding::Matrix4x2:
            case Binding::Matrix4x3:
            case Binding::Matrix4x4:
            case Binding::Vector4: return 4;
            default: return 1;
        }
    }

    class MatrixWidgetItemDelegate : public QStyledItemDelegate
    {
    public:
        MatrixWidgetItemDelegate(QWidget *parent)
            : QStyledItemDelegate(parent) {
        }

    protected:
        QWidget *createEditor(QWidget *parent, const QStyleOptionViewItem&,
                              const QModelIndex&) const override {
            auto editor = new ExpressionEditor(parent);
            editor->setFrameShape(QFrame::NoFrame);
            editor->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
            editor->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);

            connect(editor,
                (&ExpressionEditor::textChanged),
                this, &MatrixWidgetItemDelegate::valueChanged);

            return editor;
        }

        void setEditorData(QWidget *editor, const QModelIndex & index) const override {
            if (!mEditing)
                static_cast<ExpressionEditor*>(editor)->setText(
                    index.model()->data(index, Qt::EditRole).toString());
        }

        void setModelData(QWidget *editor, QAbstractItemModel *model,
                          const QModelIndex &index) const override {
            model->setData(index,
                static_cast<ExpressionEditor*>(editor)->text());
        }

        bool helpEvent(QHelpEvent *event, QAbstractItemView *view,
            const QStyleOptionViewItem& option, const QModelIndex& index )
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
        void valueChanged() {
            mEditing = true;
            emit commitData(static_cast<ExpressionEditor*>(QObject::sender()));
            mEditing = false;

            Singletons::synchronizeLogic().update();
        }

        bool mEditing{ };
    };
} // namespace

class MatrixWidget : public QTableWidget
{
public:
    explicit MatrixWidget(QWidget *parent = 0) : QTableWidget(parent)
    {
        setItemDelegate(new MatrixWidgetItemDelegate(this));
    }

    void setRowCount(int rows)
    {
        if (rows != rowCount()) {
            QTableWidget::setRowCount(rows);
            updateCells();
        }
    }

    void setColumnCount(int columns)
    {
        if (columns != colorCount()) {
            QTableWidget::setColumnCount(columns);
            updateCells();
        }
    }

    void setFields(QStringList fields)
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

    QStringList fields() const
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

protected:
    void resizeEvent(QResizeEvent *event) override
    {
        QTableWidget::resizeEvent(event);
        updateCells();
    }

    void focusInEvent(QFocusEvent *event) override
    {
        QTableWidget::focusInEvent(event);
        if (currentIndex().isValid())
            edit(currentIndex());
    }

private:
    void updateCells()
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
};

#include "ui_BindingProperties.h"

BindingProperties::BindingProperties(SessionProperties *sessionProperties)
    : QWidget(sessionProperties)
    , mSessionProperties(*sessionProperties)
    , mUi(new Ui::BindingProperties)
{
    mUi->setupUi(this);

    fill<Binding::Type>(mUi->type, {
        { "Scalar", Binding::Scalar },
        { "Vector 2", Binding::Vector2 },
        { "Vector 3", Binding::Vector3 },
        { "Vector 4", Binding::Vector4 },
        { "Matrix 2x2", Binding::Matrix2x2 },
        { "Matrix 2x3", Binding::Matrix2x3 },
        { "Matrix 2x4", Binding::Matrix2x4 },
        { "Matrix 3x2", Binding::Matrix3x2 },
        { "Matrix 3x3", Binding::Matrix3x3 },
        { "Matrix 3x4", Binding::Matrix3x4 },
        { "Matrix 4x2", Binding::Matrix4x2 },
        { "Matrix 4x3", Binding::Matrix4x3 },
        { "Matrix 4x4", Binding::Matrix4x4 },
        { "Texture", Binding::Texture },
        { "Sampler", Binding::Sampler },
        { "Image", Binding::Image },
        { "Buffer", Binding::Buffer },
        //{ "Subroutine", Binding::Subroutine },
    });

    connect(mUi->type, &DataComboBox::currentDataChanged,
        this, &BindingProperties::updateWidgets);
    using Overload = void(QSpinBox::*)(int);
    connect(mUi->count, static_cast<Overload>(&QSpinBox::valueChanged),
        this, &BindingProperties::updateWidgets);
    connect(mUi->index, &QTabWidget::currentChanged,
        this, &BindingProperties::updateWidgets);
    connect(mUi->value, &MatrixWidget::itemChanged,
        [this]() { updateValue(mUi->value->fields()); });
    connect(mUi->reference, &ReferenceComboBox::currentDataChanged,
        [this](QVariant id) { updateValue({ id.toString() }); });
    connect(mUi->reference, &ReferenceComboBox::textRequired,
        [this](QVariant id) {
            return mSessionProperties.model().findItemName(id.toInt());
        });
    connect(mUi->reference, &ReferenceComboBox::listRequired,
        this, &BindingProperties::getItemIds);

    updateWidgets();
}

BindingProperties::~BindingProperties()
{
    delete mUi;
}

QWidget *BindingProperties::typeWidget() const { return mUi->type; }

Binding::Type BindingProperties::currentType() const
{
    return static_cast<Binding::Type>(mUi->type->currentData().toInt());
}

void BindingProperties::setValues(const QVariantList &values)
{
    if (mValues != values) {
        mValues = values;
        mUi->count->setValue(mValues.size());
        updateWidgets();
    }
}

void BindingProperties::updateValue(QStringList fields)
{
    if (mSuspendUpdateValue)
        return;

    const auto index = mUi->index->currentIndex();
    mValues[index] = fields;
    emit valuesChanged(mValues);
}

void BindingProperties::updateWidgets()
{
    mSuspendUpdateValue = true;

    const auto count = mUi->count->value();
    while (mUi->index->count() > count)
        mUi->index->removeTab(mUi->index->count() - 1);
    for (auto i = mUi->index->count(); i < mUi->count->value(); ++i)
        mUi->index->addTab(new QWidget(), QString::number(i));
    mUi->index->setVisible(count > 1);

    while (mValues.size() < count)
        mValues.push_back(QVariantList());
    while (mValues.size() > count)
        mValues.removeLast();

    const auto type = currentType();
    const auto isTexture = (type == Binding::Texture);
    const auto isSampler = (type == Binding::Sampler);
    const auto isImage = (type == Binding::Image);
    const auto isBuffer = (type == Binding::Buffer);
    const auto isReference = (isTexture || isSampler || isImage || isBuffer);

    if (!isReference) {
        mUi->value->setColumnCount(typeColumns(type));
        mUi->value->setRowCount(typeRows(type));
        mUi->value->setFields(mValues[mUi->index->currentIndex()].toStringList());
    }
    else {
        const auto list = mValues[mUi->index->currentIndex()].toList();
        const auto id = (!list.isEmpty() ? list.first().toInt() : 0);
        mUi->reference->setCurrentData(id);
        mUi->reference->validate();
    }
    mUi->reference->setVisible(isReference);
    mUi->value->setVisible(!isReference);
    mUi->textureLevel->setVisible(isImage);
    mUi->labelLevel->setVisible(isImage);

    mSuspendUpdateValue = false;
}

QVariantList BindingProperties::getItemIds() const
{
    const auto bindingType = currentType();
    const auto referenceType =
        (bindingType == Binding::Texture ? ItemType::Texture :
         bindingType == Binding::Sampler ? ItemType::Sampler :
         bindingType == Binding::Image ? ItemType::Texture :
         ItemType::Buffer);

    return mSessionProperties.getItemIds(referenceType);
}
