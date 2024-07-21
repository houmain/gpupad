#pragma once

#include <QComboBox>

class ReferenceComboBox final : public QComboBox
{
    Q_OBJECT
    Q_PROPERTY(QVariant currentData READ currentData WRITE
        setCurrentData NOTIFY currentDataChanged USER true)
public:
    explicit ReferenceComboBox(QWidget *parent = nullptr);
    void setCurrentData(QVariant data);
    void validate();

    QSize minimumSizeHint() const override;
    void showPopup() override;
    void showEvent(QShowEvent *event) override;

Q_SIGNALS:
    QVariantList listRequired();
    QString textRequired(QVariant data);
    void currentDataChanged(QVariant data);

protected:
    void paintEvent(QPaintEvent *event) override;

private:
    void refreshList();
    void insertItem(int index, QVariant data);

    bool mSuspendDataChangedSignal{ };
};

