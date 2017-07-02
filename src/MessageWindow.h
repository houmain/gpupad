#ifndef MESSAGEWINDOW_H
#define MESSAGEWINDOW_H

#include <QTableWidget>
#include <QMutex>

class QTimer;

class MessageWindow : public QTableWidget
{
    Q_OBJECT

public:
    explicit MessageWindow(QWidget *parent = 0);

public slots:
    void handleMessageChanged();

signals:
    void messageActivated(int itemId, QString fileName, int line, int column);

private slots:
    void updateItems();
    void handleItemActivated(QTableWidgetItem *item);

protected:
    QStyleOptionViewItem viewOptions() const override;

private:
    QTimer *mUpdateItemsTimer;
    QIcon mWarningIcon;
};

#endif // MESSAGEWINDOW_H
