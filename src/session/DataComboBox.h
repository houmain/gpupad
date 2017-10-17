#ifndef DATACOMBOBOX_H
#define DATACOMBOBOX_H

#include <QComboBox>

class DataComboBox : public QComboBox
{
    Q_OBJECT
    Q_PROPERTY(QVariant currentData READ currentData WRITE
        setCurrentData NOTIFY currentDataChanged USER true)
public:
    explicit DataComboBox(QWidget *parent = nullptr);
    void setCurrentData(QVariant data);

    QSize minimumSizeHint() const override;

signals:
    void currentDataChanged(QVariant data);
};

#endif // DATACOMBOBOX_H
