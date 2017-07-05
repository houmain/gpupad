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

protected:
    QStyleOptionViewItem viewOptions() const override;

private:
    static MessageId getMessageId(const Message &message);
    QString getMessageText(const Message &message) const;
    void removeMessagesExcept(const QSet<MessageId> &messageIds);
    void addMessage(const Message &message);

    QTimer *mUpdateItemsTimer;
    QIcon mWarningIcon;
    QSet<MessageId> mMessageIds;
};

#endif // MESSAGEWINDOW_H
