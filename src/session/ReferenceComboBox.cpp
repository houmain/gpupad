#include "ReferenceComboBox.h"
#include <QStandardItemModel>
#include <QStylePainter>

ReferenceComboBox::ReferenceComboBox(QWidget *parent) : QComboBox(parent)
{
    connect(this, qOverload<int>(&QComboBox::currentIndexChanged), [this]() {
        if (!mSuspendDataChangedSignal)
            Q_EMIT currentDataChanged(currentData());
    });
}

void ReferenceComboBox::setCurrentData(QVariant data)
{
    auto index = findData(data);
    if (index < 0) {
        insertItem(0, data);
        index = 0;
    }
    setCurrentIndex(index);
}

void ReferenceComboBox::validate()
{
    if (!listRequired().contains(currentData()))
        setCurrentIndex(-1);
}

QSize ReferenceComboBox::minimumSizeHint() const
{
    return QSize(50, QComboBox::minimumSizeHint().height());
}

void ReferenceComboBox::showPopup()
{
    refreshList();
    QComboBox::showPopup();
}

void ReferenceComboBox::showEvent(QShowEvent *event)
{
    if (currentIndex() != -1)
        setItemText(currentIndex(), textRequired(currentData()));
    QComboBox::showEvent(event);
}

void ReferenceComboBox::paintEvent(QPaintEvent *event)
{
    QStyleOptionComboBox opt;
    initStyleOption(&opt);

    // convert full to plain item name
    auto text = opt.currentText;
    const auto fullwidthHyphenMinus = QChar(0xFF0D);
    if (auto i = text.lastIndexOf(fullwidthHyphenMinus); i >= 0)
        text = text.mid(i + 1);

    // still show complete text in tooltip
    if (opt.currentText != text) {
        setToolTip(opt.currentText);
        opt.currentText = text;
    } else {
        setToolTip("");
    }

    QStylePainter p(this);
    p.drawComplexControl(QStyle::CC_ComboBox, opt);
    p.drawControl(QStyle::CE_ComboBoxLabel, opt);
}

void ReferenceComboBox::refreshList()
{
    mSuspendDataChangedSignal = true;
    auto current = currentData();

    clear();
    const auto list = Q_EMIT listRequired();
    for (const QVariant &data : list)
        insertItem(model()->rowCount(), data);

    setCurrentData(current);
    mSuspendDataChangedSignal = false;
}

void ReferenceComboBox::insertItem(int index, QVariant data)
{
    if (auto model = qobject_cast<QStandardItemModel *>(this->model())) {
        const auto text = Q_EMIT textRequired(data);
        auto item = new QStandardItem(text);
        item->setData(data, Qt::UserRole);
        model->insertRow(index, item);
    }
}
