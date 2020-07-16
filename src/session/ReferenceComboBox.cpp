#include "ReferenceComboBox.h"
#include <QStandardItemModel>

ReferenceComboBox::ReferenceComboBox(QWidget *parent) : QComboBox(parent)
{
    connect(this, qOverload<int>(&QComboBox::currentIndexChanged),
        [this]() {
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

void ReferenceComboBox::refreshList()
{
    mSuspendDataChangedSignal = true;
    auto current = currentData();

    clear();
    for (QVariant data : listRequired())
        insertItem(model()->rowCount(), data);

    setCurrentData(current);
    mSuspendDataChangedSignal = false;
}

void ReferenceComboBox::insertItem(int index, QVariant data)
{
    if (auto model = qobject_cast<QStandardItemModel*>(this->model())) {
        auto item = new QStandardItem(textRequired(data));
        item->setData(data, Qt::UserRole);
        model->insertRow(index, item);
    }
}
