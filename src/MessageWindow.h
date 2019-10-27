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
    explicit MessageWindow(QWidget *parent = nullptr);

signals:
    void messageActivated(int itemId, QString fileName, int line, int column);
    void messagesAdded();

private slots:
    void updateMessages();
    void handleItemActivated(QTableWidgetItem *item);

private:
    QIcon getMessageIcon(const Message &message) const;
    QString getMessageText(const Message &message) const;
    QString getLocationText(const Message &message) const;
    void removeMessagesExcept(const QSet<MessageId> &messageIds);
    MessageId tryReplaceMessage(const Message &message);
    bool addMessageOnce(const Message &message);

    QTimer *mUpdateItemsTimer;
    QIcon mInfoIcon;
    QIcon mWarningIcon;
    QIcon mErrorIcon;
    QSet<MessageId> mMessageIds;
};

#endif // MESSAGEWINDOW_H
