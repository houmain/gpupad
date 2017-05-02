#ifndef REFERENCECOMBOBOX_H
#define REFERENCECOMBOBOX_H

#include <QComboBox>

class ReferenceComboBox : public QComboBox
{
    Q_OBJECT
    Q_PROPERTY(QVariant currentData READ currentData WRITE
        setCurrentData NOTIFY currentDataChanged USER true)
public:
    explicit ReferenceComboBox(QWidget *parent = 0);
    void setCurrentData(QVariant data);
    void validate();

    QSize minimumSizeHint() const override;
    void showPopup() override;
    void showEvent(QShowEvent *event) override;

signals:
    QVariantList listRequired();
    QString textRequired(QVariant data);
    void currentDataChanged(QVariant data);

private:
    void refreshList();
    void insertItem(int index, QVariant data);

    bool mSuspendDataChangedSignal{ };
};

#endif // REFERENCECOMBOBOX_H
