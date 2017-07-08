#include "MessageWindow.h"
#include "Singletons.h"
#include "MessageList.h"
#include "session/SessionModel.h"
#include "FileDialog.h"
#include <QTimer>
#include <QHeaderView>

MessageWindow::MessageWindow(QWidget *parent) : QTableWidget(parent)
{
    connect(this, &MessageWindow::itemActivated,
        this, &MessageWindow::handleItemActivated);

    mUpdateItemsTimer = new QTimer(this);
    mUpdateItemsTimer->setInterval(250);
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

    mInfoIcon.addFile(QStringLiteral(":/images/16x16/dialog-information.png"));
    mWarningIcon.addFile(QStringLiteral(":/images/16x16/dialog-warning.png"));
    mErrorIcon.addFile(QStringLiteral(":/images/16x16/dialog-error.png"));
}

void MessageWindow::updateMessages()
{
    auto messages = Singletons::messageList().messages();

    auto messageIds = QSet<MessageId>();
    foreach (const MessagePtr &message, messages) {

        if (message->type == MessageType::CallDuration)
            tryReplaceMessage(*message);

        messageIds += getMessageId(*message);
    }
    removeMessagesExcept(messageIds);

    foreach (const MessagePtr& message, messages)
        addMessageOnce(*message);
}

qulonglong MessageWindow::getMessageId(const Message &message)
{
    return static_cast<qulonglong>(reinterpret_cast<uintptr_t>(&message));
}

QIcon MessageWindow::getMessageIcon(const Message &message) const
{
    switch (message.type) {
        case OpenGL33NotSupported:
        case LoadingFileFailed:
        case UnsupportedShaderType:
        case CreatingFramebufferFailed:
        case DownloadingImageFailed:
        case ShaderError:
        case ScriptError:
            return mErrorIcon;

        case ShaderWarning:
        case UnformNotSet:
            return mWarningIcon;

        case ShaderInfo:
        case CallDuration:
            return mInfoIcon;
    }
    return mWarningIcon;
}

QString MessageWindow::getMessageText(const Message &message) const
{
    switch (message.type) {
        case ShaderInfo:
        case ShaderWarning:
        case ShaderError:
        case ScriptError:
            return message.text;

        case OpenGL33NotSupported:
            return tr("the minimum required OpenGL version 3.3 is not supported");
        case LoadingFileFailed:
            return tr("loading file '%1' failed").arg(message.text);
        case UnsupportedShaderType:
            return tr("unsupported shader type");
        case CreatingFramebufferFailed:
            return tr("creating framebuffer failed");
        case DownloadingImageFailed:
            return tr("downloading image failed");
        case UnformNotSet:
            return tr("uniform '%1' not set").arg(message.text);
        case CallDuration:
            return tr("call took %1").arg(message.text);
    }
    return message.text;
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

void MessageWindow::tryReplaceMessage(const Message &message)
{
    for (auto i = 0; i < rowCount(); i++) {
        auto& item = *this->item(i, 0);
        if (item.data(Qt::UserRole + 1) == message.itemId &&
            item.data(Qt::UserRole + 4) == message.type) {

            item.setData(Qt::UserRole, getMessageId(message));
            item.setText(getMessageText(message));
            return;
        }
    }
}

void MessageWindow::addMessageOnce(const Message &message)
{
    auto messageId = getMessageId(message);
    for (auto i = 0; i < rowCount(); i++)
        if (item(i, 0)->data(Qt::UserRole).toULongLong() == messageId)
            return;

    auto locationText = QString();
    if (message.itemId) {
        auto item = Singletons::sessionModel().findItem(message.itemId);
        if (item->parent && !item->parent->name.isEmpty())
            locationText += item->parent->name + " ";
        locationText += item->name;
    }
    else if (!message.fileName.isEmpty()) {
        locationText = FileDialog::getFileTitle(message.fileName);
        if (message.line > 0)
            locationText += ":" + QString::number(message.line);
    }

    auto messageIcon = getMessageIcon(message);
    auto messageText = getMessageText(message);
    auto messageItem = new QTableWidgetItem(messageIcon, messageText);
    messageItem->setData(Qt::UserRole, messageId);
    messageItem->setData(Qt::UserRole + 1, message.itemId);
    messageItem->setData(Qt::UserRole + 2, message.fileName);
    messageItem->setData(Qt::UserRole + 3, message.line);
    messageItem->setData(Qt::UserRole + 4, message.type);

    auto locationItem = new QTableWidgetItem(locationText);
    locationItem->setTextAlignment(Qt::AlignLeft | Qt::AlignTop);

    auto row = rowCount();
    insertRow(row);
    setItem(row, 0, messageItem);
    setItem(row, 1, locationItem);
}

void MessageWindow::handleItemActivated(QTableWidgetItem *messageItem)
{
    auto itemId = messageItem->data(Qt::UserRole + 1).toInt();
    auto fileName = messageItem->data(Qt::UserRole + 2).toString();
    auto line = messageItem->data(Qt::UserRole + 3).toInt();
    emit messageActivated(itemId, fileName, line, -1);
}
