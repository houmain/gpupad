#include "MessageWindow.h"
#include "files/FileDialog.h"
#include <QTimer>
#include <QHeaderView>

MessageWindow::MessageWindow(QWidget *parent) : QTableWidget(parent)
{
    connect(this, &MessageWindow::messagesChanged,
        this, &MessageWindow::handleMessagesChanged);
    connect(this, &MessageWindow::itemActivated,
        this, &MessageWindow::handleItemActivated);

    mUpdateItemsTimer = new QTimer(this);
    mUpdateItemsTimer->setSingleShot(true);
    mUpdateItemsTimer->setInterval(100);
    connect(mUpdateItemsTimer, &QTimer::timeout,
        this, &MessageWindow::updateItems);

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

void MessageWindow::insertMessage(Message *message)
{
    QMutexLocker lock(&mMutex);
    mMessages.append(message);
    emit messagesChanged();
}

void MessageWindow::removeMessage(Message *message)
{
    QMutexLocker lock(&mMutex);
    Q_ASSERT(mMessages.contains(message));
    mMessages.removeAll(message);
    emit messagesChanged();
}

void MessageWindow::handleMessagesChanged()
{
    mUpdateItemsTimer->start();
}

void MessageWindow::updateItems()
{
    auto icon = QIcon();
    icon.addFile(QStringLiteral(":/images/16x16/dialog-warning.png"));

    clear();

    QMutexLocker lock(&mMutex);
    setRowCount(mMessages.size());
    auto alreadyAdded = QSet<QString>();
    auto row = 0;
    foreach (const Message* message, mMessages) {
        auto locationText = QString();
        if (!message->fileName.isEmpty())
            locationText = FileDialog::getFileTitle(message->fileName);
        if (message->line >= 0)
            locationText += ":" + QString::number(message->line);

        // prevent adding duplicates
        const auto line = locationText + message->text;
        if (alreadyAdded.contains(line))
            continue;
        alreadyAdded.insert(line);

        auto messageText = message->text.trimmed();
        switch (message->type) {
            case Error:
            case Warning:
            case Info:
                break;
            case OpenGL33NotSupported:
                messageText = tr("the minimum required OpenGL version 3.3 is not supported");
                break;
            case LoadingFileFailed:
                messageText = tr("loading file '%1' failed").arg(messageText);
                break;
            case UnsupportedShaderType:
                messageText = tr("unsupported shader type");
                break;
        }

        auto messageItem = new QTableWidgetItem(icon, messageText);
        messageItem->setData(Qt::UserRole + 0, message->itemId);
        messageItem->setData(Qt::UserRole + 1, message->fileName);
        messageItem->setData(Qt::UserRole + 2, message->line);
        messageItem->setData(Qt::UserRole + 3, message->column);

        auto locationItem = new QTableWidgetItem(locationText);
        locationItem->setTextAlignment(Qt::AlignLeft | Qt::AlignTop);

        setItem(row, 0, messageItem);
        setItem(row, 1, locationItem);
        row++;
    }
    setRowCount(row);
}

void MessageWindow::handleItemActivated(QTableWidgetItem *listItem)
{
    auto itemId = listItem->data(Qt::UserRole + 0).toInt();
    auto fileName = listItem->data(Qt::UserRole + 1).toString();
    auto line = listItem->data(Qt::UserRole + 2).toInt();
    auto column = listItem->data(Qt::UserRole + 3).toInt();
    emit messageActivated(itemId, fileName, line, column);
}
