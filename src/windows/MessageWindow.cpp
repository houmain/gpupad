#include "MessageWindow.h"
#include "FileDialog.h"
#include "MessageList.h"
#include "editors/EditorManager.h"
#include "editors/source/SourceEditor.h"
#include "Singletons.h"
#include "WindowTitle.h"
#include "session/SessionModel.h"
#include <QHeaderView>
#include <QRegularExpression>
#include <QStandardItemModel>
#include <QTimer>
#include <QToolButton>
#include <QVBoxLayout>
#include <QTextStream>

MessageWindow::MessageWindow(QWidget *parent)
    : QTableWidget(parent)
    , mExportButton(new QToolButton(this))
{
    connect(this, &MessageWindow::itemActivated, this,
        &MessageWindow::handleItemActivated);

    mUpdateItemsTimer = new QTimer(this);
    connect(mUpdateItemsTimer, &QTimer::timeout, this,
        &MessageWindow::updateMessages);
    mUpdateItemsTimer->setSingleShot(true);
    mUpdateItemsTimer->start();

    setColumnCount(2);
    verticalHeader()->setVisible(false);
    horizontalHeader()->setVisible(false);
    verticalHeader()->setSectionResizeMode(QHeaderView::ResizeToContents);
    verticalHeader()->setDefaultSectionSize(20);
    horizontalHeader()->setSectionResizeMode(0, QHeaderView::Stretch);
    horizontalHeader()->setSectionResizeMode(1, QHeaderView::ResizeToContents);
    setEditTriggers(NoEditTriggers);
    setSelectionMode(SingleSelection);
    setSelectionBehavior(SelectRows);
    setVerticalScrollMode(ScrollPerPixel);
    setShowGrid(false);
    setAlternatingRowColors(true);
    setWordWrap(true);

    mInfoIcon = QIcon::fromTheme("dialog-information");
    mWarningIcon = QIcon::fromTheme("dialog-warning");
    mErrorIcon = QIcon::fromTheme("dialog-error");

    mExportButton->setIcon(
        QIcon(QIcon::fromTheme(QString::fromUtf8("application-exit"))));
    mExportButton->setToolTip(tr("Export To Editor"));
    mExportButton->setAutoRaise(true);

    auto header = new QWidget(this);
    auto headerLayout = new QHBoxLayout(header);
    headerLayout->setContentsMargins(4, 4, 4, 4);
    headerLayout->addWidget(mExportButton);
    headerLayout->addStretch(1);

    auto titleBar = new WindowTitle();
    titleBar->setWidget(header);
    mTitleBar = titleBar;

    connect(mExportButton, &QToolButton::clicked, this,
        &MessageWindow::exportMessages);
}

void MessageWindow::updateMessages()
{
    auto added = false;
    auto messages = MessageList::messages();
    auto messageIds = QSet<MessageId>();
    for (auto it = messages.begin(); it != messages.end();) {
        const auto &message = **it;
        added |= addMessageOnce(message);
        messageIds += message.id;
        ++it;
    }
    removeMessagesExcept(messageIds);

    if (added) {
        resizeRowsToContents();
        Q_EMIT messagesAdded();
    }
    mUpdateItemsTimer->start(50);
}

QIcon MessageWindow::getMessageIcon(const Message &message) const
{
    switch (getMessageSeverity(message)) {
    case MessageSeverity::Error:   return mErrorIcon;
    case MessageSeverity::Warning: return mWarningIcon;
    case MessageSeverity::Info:    return mInfoIcon;
    }
    return {};
}

QString MessageWindow::getLocationText(const Message &message) const
{
    auto locationText = QString();
    if (message.itemId) {
        locationText +=
            Singletons::sessionModel().getFullItemName(message.itemId);
    } else if (!message.fileName.isEmpty()) {
        locationText = FileDialog::getFileTitle(message.fileName);
        if (message.line > 0)
            locationText += ":" + QString::number(message.line);
    }
    return locationText;
}

void MessageWindow::removeMessagesExcept(const QSet<MessageId> &messageIds)
{
    for (auto row = 0; row < mMessageIds.size();)
        if (!messageIds.contains(mMessageIds[row])) {
            removeRow(row);
            mMessageIds.removeAt(row);
        } else {
            ++row;
        }
}

bool MessageWindow::addMessageOnce(const Message &message)
{
    auto it =
        std::lower_bound(mMessageIds.begin(), mMessageIds.end(), message.id);
    if (it != mMessageIds.end() && *it == message.id)
        return false;
    const auto row = std::distance(mMessageIds.begin(), it);
    mMessageIds.insert(it, message.id);

    auto messageIcon = getMessageIcon(message);
    auto messageText = getMessageText(message);
    auto messageItem = new QTableWidgetItem(messageIcon, messageText);
    messageItem->setData(Qt::UserRole + 1, message.itemId);
    messageItem->setData(Qt::UserRole + 2, message.fileName);
    messageItem->setData(Qt::UserRole + 3, message.line);
    messageItem->setData(Qt::UserRole + 4, static_cast<int>(message.type));

    auto locationText = getLocationText(message);
    auto locationItem = new QTableWidgetItem(locationText);
    locationItem->setTextAlignment(Qt::AlignLeft | Qt::AlignTop);

    insertRow(row);
    setItem(row, 0, messageItem);
    setItem(row, 1, locationItem);
    return true;
}

void MessageWindow::handleItemActivated(QTableWidgetItem *messageItem)
{
    auto itemId = messageItem->data(Qt::UserRole + 1).toInt();
    auto fileName = messageItem->data(Qt::UserRole + 2).toString();
    auto line = messageItem->data(Qt::UserRole + 3).toInt();

    // parse line number from text
    static const auto regex = QRegularExpression("in line (\\d+)",
        QRegularExpression::MultilineOption);
    if (auto match = regex.match(messageItem->text()); match.hasMatch())
        line = match.capturedView(1).toInt();

    Q_EMIT messageActivated(itemId, fileName, line, -1);
}

void MessageWindow::exportMessages()
{
    auto string = QString();
    auto out = QTextStream(&string);
    for (auto row = 0; row < rowCount(); ++row)
        out << item(row, 0)->text() << "\t" << item(row, 1)->text() << "\n";

    auto editor =
        Singletons::editorManager().getSourceEditor(mLastExportFileName);
    if (!editor)
        mLastExportFileName =
            FileDialog::generateNextUntitledFileName("Messages");
    editor = Singletons::editorManager().openSourceEditor(mLastExportFileName);
    if (editor)
        editor->replace(string);
}
