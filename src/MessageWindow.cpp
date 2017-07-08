#include "MessageWindow.h"
#include "Singletons.h"
#include "MessageList.h"
#include "session/SessionModel.h"
#include "FileDialog.h"
#include <QTimer>
#include <QHeaderView>

MessageWindow::MessageWindow(QWidget *parent) : QTableWidget(parent)
{
    mWarningIcon.addFile(QStringLiteral(":/images/16x16/dialog-warning.png"));

    connect(this, &MessageWindow::itemActivated,
        this, &MessageWindow::handleItemActivated);

    mUpdateItemsTimer = new QTimer(this);
    mUpdateItemsTimer->setInterval(50);
    connect(mUpdateItemsTimer, &QTimer::timeout,
        this, &MessageWindow::updateMessages);
    mUpdateItemsTimer->start();

    setColumnCount(2);
    verticalHeader()->setVisible(false);
    horizontalHeader()->setVisible(false);
    verticalHeader()->setSectionResizeMode(QHeaderView::ResizeToContents);
    horizontalHeader()->setSectionResizeMode(0, QHeaderView::Stretch);
    horizontalHeader()->setSectionResizeMode(1, QHeaderView::ResizeToContents);
    setEditTriggers(NoEditTriggers);
    setSelectionMode(SingleSelection);
    setSelectionBehavior(SelectRows);
    setVerticalScrollMode(ScrollPerPixel);
    setShowGrid(false);
    setAlternatingRowColors(true);
    setWordWrap(true);
}

QStyleOptionViewItem MessageWindow::viewOptions() const
{
    auto option = QTableWidget::viewOptions();
    option.decorationAlignment = Qt::AlignHCenter | Qt::AlignTop;
    return option;
}

void MessageWindow::updateMessages()
{
    auto messages = Singletons::messageList().messages();

    auto messageIds = QSet<MessageId>();
    foreach (const MessagePtr &message, messages)
        messageIds += getMessageId(*message);
    removeMessagesExcept(messageIds);

    foreach (const MessagePtr& message, messages)
        if (!mMessageIds.contains(getMessageId(*message)))
            addMessage(*message);
}

qulonglong MessageWindow::getMessageId(const Message &message)
{
    return static_cast<qulonglong>(reinterpret_cast<uintptr_t>(&message));
}

QString MessageWindow::getMessageText(const Message &message) const
{
    auto messageText = message.text.trimmed();
    switch (message.type) {
        case Error:
        case Warning:
        case Info:
            break;
        case OpenGL33NotSupported:
            return tr("the minimum required OpenGL version 3.3 is not supported");
        case LoadingFileFailed:
            return tr("loading file '%1' failed").arg(messageText);
        case UnsupportedShaderType:
            return tr("unsupported shader type");
        case CreatingFramebufferFailed:
            return tr("creating framebuffer failed");
        case UnformNotSet:
            return tr("uniform '%1' not set").arg(messageText);
    }
    return messageText;
}

void MessageWindow::removeMessagesExcept(const QSet<MessageId> &messageIds)
{
    for (auto i = 0; i < rowCount(); ) {
        auto it = item(i, 0);
        if (!messageIds.contains(it->data(Qt::UserRole).toULongLong()))
            removeRow(i);
        else
            ++i;
    }
}

void MessageWindow::addMessage(const Message &message)
{
    auto locationText = QString();
    if (message.itemId) {
        locationText = Singletons::sessionModel().findItemName(message.itemId);
    }
    else if (!message.fileName.isEmpty()) {
        locationText = FileDialog::getFileTitle(message.fileName);
        if (message.line > 0)
            locationText += ":" + QString::number(message.line);
    }

    auto messageText = getMessageText(message);
    auto messageId = getMessageId(message);
    auto messageItem = new QTableWidgetItem(mWarningIcon, messageText);
    messageItem->setData(Qt::UserRole, messageId);
    messageItem->setData(Qt::UserRole + 1, message.itemId);
    messageItem->setData(Qt::UserRole + 2, message.fileName);
    messageItem->setData(Qt::UserRole + 3, message.line);

    auto locationItem = new QTableWidgetItem(locationText);
    locationItem->setTextAlignment(Qt::AlignLeft | Qt::AlignTop);

    auto row = rowCount();
    insertRow(row);
    setItem(row, 0, messageItem);
    setItem(row, 1, locationItem);
    mMessageIds += messageId;
}

void MessageWindow::handleItemActivated(QTableWidgetItem *listItem)
{
    auto itemId = listItem->data(Qt::UserRole + 1).toInt();
    auto fileName = listItem->data(Qt::UserRole + 2).toString();
    auto line = listItem->data(Qt::UserRole + 3).toInt();
    emit messageActivated(itemId, fileName, line, -1);
}
