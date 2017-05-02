#ifndef MESSAGEWINDOW_H
#define MESSAGEWINDOW_H

#include "MessageList.h"
#include <QTableWidget>
#include <QMutex>

class QTimer;

class MessageWindow : public QTableWidget
{
    Q_OBJECT

public:
    explicit MessageWindow(QWidget *parent = 0);

signals:
    void messagesChanged();
    void messageActivated(int itemId, QString fileName, int line, int column);

protected:
    QStyleOptionViewItem viewOptions() const override;

private:
    void handleMessagesChanged();
    void handleItemActivated(QTableWidgetItem *item);
    void updateItems();

    friend class MessageList;
    void insertMessage(Message *message);
    void removeMessage(Message *message);

    QTimer *mUpdateItemsTimer;
    QMutex mMutex;
    QList<Message*> mMessages;
};

#endif // MESSAGEWINDOW_H
