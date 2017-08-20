#ifndef MESSAGEWINDOW_H
#define MESSAGEWINDOW_H

#include <QTableWidget>
#include <QMutex>

struct Message;
class QTimer;

class MessageWindow : public QTableWidget
{
    Q_OBJECT
    using MessageId = qulonglong;

public:
    explicit MessageWindow(QWidget *parent = 0);

signals:
    void messageActivated(int itemId, QString fileName, int line, int column);
    void messagesAdded();

private slots:
    void updateMessages();
    void handleItemActivated(QTableWidgetItem *item);

private:
    QIcon getMessageIcon(const Message &message) const;
    QString getMessageText(const Message &message) const;
    void removeMessagesExcept(const QSet<MessageId> &messageIds);
    void tryReplaceMessage(const Message &message);
    bool addMessageOnce(const Message &message);

    QTimer *mUpdateItemsTimer;
    QIcon mInfoIcon;
    QIcon mWarningIcon;
    QIcon mErrorIcon;
};

#endif // MESSAGEWINDOW_H
