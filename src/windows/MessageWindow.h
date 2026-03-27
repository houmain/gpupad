#pragma once

#include "MessageList.h"
#include <QTableWidget>

class QTimer;
class QToolButton;

class MessageWindow final : public QTableWidget
{
    Q_OBJECT

public:
    explicit MessageWindow(QWidget *parent = nullptr);
    QWidget *titleBar() const { return mTitleBar; }

Q_SIGNALS:
    void messageActivated(int itemId, QString fileName, int line, int column);
    void messagesAdded();

private:
    void updateMessages();
    void handleItemActivated(QTableWidgetItem *item);
    QIcon getMessageIcon(const Message &message) const;
    QString getLocationText(const Message &message) const;
    void removeMessagesExcept(const QSet<MessageId> &messageIds);
    bool addMessageOnce(const Message &message);
    void exportMessages();

    QWidget *mTitleBar{};
    QTimer *mUpdateItemsTimer;
    QIcon mInfoIcon;
    QIcon mWarningIcon;
    QIcon mErrorIcon;
    QList<MessageId> mMessageIds;
    QToolButton *mExportButton{};
    QString mLastExportFileName;
};
