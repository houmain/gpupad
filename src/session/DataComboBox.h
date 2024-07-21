#pragma once

#include <QComboBox>

class DataComboBox final : public QComboBox
{
    Q_OBJECT
    Q_PROPERTY(QVariant currentData READ currentData WRITE
        setCurrentData NOTIFY currentDataChanged USER true)
public:
    explicit DataComboBox(QWidget *parent = nullptr);
    void setCurrentData(QVariant data);

    QSize minimumSizeHint() const override;

Q_SIGNALS:
    void currentDataChanged(QVariant data);
};

