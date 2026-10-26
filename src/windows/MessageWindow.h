#pragma once

#include "message/MessageList.h"
#include <QTableView>
#include <memory>

class QTimer;
class QToolButton;
class MessageTableModel;
class MessageLocationIndex;
struct MessageSnapshot;

class MessageWindow final : public QTableView
{
    Q_OBJECT

public:
    explicit MessageWindow(QWidget *parent = nullptr);
    ~MessageWindow() override;
    QWidget *titleBar() const { return mTitleBar; }

Q_SIGNALS:
    void messageActivated(int itemId, QString fileName, int line, int column);
    void messagesAdded();

private:
    void checkForUpdates();
    void startMessagesUpdate(MessagesSnapshot messages);
    void applySnapshot(std::shared_ptr<const MessageSnapshot> snapshot,
        qulonglong messagesRevision, qulonglong locationRevision,
        bool messagesAdded);
    void handleItemActivated(const QModelIndex &index);
    void exportMessages();
    void connectSessionModel();
    void invalidateLocations();

    QWidget *mTitleBar{};
    QTimer *mUpdateItemsTimer{};
    std::unique_ptr<MessageTableModel> mModel;
    QToolButton *mExportButton{};
    QString mLastExportFileName;
    qulonglong mAppliedMessagesRevision{};
    qulonglong mLocationRevision{ 1 };
    qulonglong mAppliedLocationRevision{};
    std::shared_ptr<const MessageLocationIndex> mCachedLocations;
    bool mUpdateInProgress{};
    bool mSessionModelConnected{};
};
