#include "DataComboBox.h"

DataComboBox::DataComboBox(QWidget *parent) : QComboBox(parent)
{
    connect(this, qOverload<int>(&QComboBox::currentIndexChanged),
        [this]() { Q_EMIT currentDataChanged(currentData()); });
}

void DataComboBox::setCurrentData(QVariant data)
{
    setCurrentIndex(findData(data));
}

QSize DataComboBox::minimumSizeHint() const
{
    return QSize(50, QComboBox::minimumSizeHint().height());
}
