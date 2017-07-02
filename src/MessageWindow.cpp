#include "MessageWindow.h"
#include "Singletons.h"
#include "MessageList.h"
#include "FileDialog.h"
#include <QTimer>
#include <QHeaderView>

MessageWindow::MessageWindow(QWidget *parent) : QTableWidget(parent)
{
    mWarningIcon.addFile(QStringLiteral(":/images/16x16/dialog-warning.png"));

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

void MessageWindow::handleMessageChanged()
{
    mUpdateItemsTimer->start();
}

void MessageWindow::updateItems()
{
    auto messages = Singletons::messageList().messages();

    setRowCount(messages.size());

    auto alreadyAdded = QSet<QString>();
    auto row = 0;
    foreach (const MessagePtr& message, messages) {
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
            case CreatingFramebufferFailed:
                messageText = tr("creating framebuffer failed");
                break;
        }

        auto messageItem = new QTableWidgetItem(mWarningIcon, messageText);
        messageItem->setData(Qt::UserRole + 0, message->itemId);
        messageItem->setData(Qt::UserRole + 1, message->fileName);
        messageItem->setData(Qt::UserRole + 2, message->line);

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
    emit messageActivated(itemId, fileName, line, -1);
}
