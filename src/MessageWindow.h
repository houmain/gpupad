#ifndef MESSAGEWINDOW_H
#define MESSAGEWINDOW_H

#include <QTableWidget>
#include <QMutex>

class Message;
class QTimer;

class MessageWindow : public QTableWidget
{
    Q_OBJECT
    using MessageId = qulonglong;

public:
    explicit MessageWindow(QWidget *parent = 0);

signals:
    void messageActivated(int itemId, QString fileName, int line, int column);

private slots:
    void updateMessages();
    void handleItemActivated(QTableWidgetItem *item);

private:
    static MessageId getMessageId(const Message &message);
    QIcon getMessageIcon(const Message &message) const;
    QString getMessageText(const Message &message) const;
    void removeMessagesExcept(const QSet<MessageId> &messageIds);
    void tryReplaceMessage(const Message &message);
    void addMessageOnce(const Message &message);

    QTimer *mUpdateItemsTimer;
    QIcon mInfoIcon;
    QIcon mWarningIcon;
    QIcon mErrorIcon;
};

#endif // MESSAGEWINDOW_H
