#ifndef MESSAGEWINDOW_H
#define MESSAGEWINDOW_H

#include "MessageList.h"
#include <QTableWidget>
#include <QMutex>

class QTimer;

class MessageWindow final : public QTableWidget
{
    Q_OBJECT

public:
    explicit MessageWindow(QWidget *parent = nullptr);

Q_SIGNALS:
    void messageActivated(int itemId, QString fileName, int line, int column);
    void messagesAdded();

private:
    void updateMessages();
    void handleItemActivated(QTableWidgetItem *item);
    QIcon getMessageIcon(const Message &message) const;
    QString getMessageText(const Message &message) const;
    QString getLocationText(const Message &message) const;
    void removeMessagesExcept(const QSet<MessageId> &messageIds);
    bool addMessageOnce(const Message &message);

    QTimer *mUpdateItemsTimer;
    QIcon mInfoIcon;
    QIcon mWarningIcon;
    QIcon mErrorIcon;
    QList<MessageId> mMessageIds;
};

#endif // MESSAGEWINDOW_H
