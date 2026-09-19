#pragma once

#include <QComboBox>

class DataComboBox final : public QComboBox
{
    Q_OBJECT
    Q_PROPERTY(QVariant currentData READ currentData WRITE setCurrentData NOTIFY
            currentDataChanged USER true)
public:
    explicit DataComboBox(QWidget *parent = nullptr);
    void setCurrentData(QVariant data);
    void setAllowNoSelection(bool allow) { mAllowNoSelection = allow; }
    bool allowNoSelection() const { return mAllowNoSelection; }

    QSize minimumSizeHint() const override;

Q_SIGNALS:
    void currentDataChanged(QVariant data);

private:
    bool mAllowNoSelection{ };
};
