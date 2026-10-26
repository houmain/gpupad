#include "MessageWindow.h"
#include "FileDialog.h"
#include "message/MessageList.h"
#include "message/MessageLocationIndex.h"
#include "editors/EditorManager.h"
#include "editors/source/SourceEditor.h"
#include "Singletons.h"
#include "WindowTitle.h"
#include "session/SessionModel.h"
#include <QAbstractTableModel>
#include <QApplication>
#include <QHeaderView>
#include <QPointer>
#include <QRegularExpression>
#include <QThreadPool>
#include <QTimer>
#include <QToolButton>
#include <QVBoxLayout>
#include <QTextStream>
#include <algorithm>

struct MessageRow
{
    Message message;
    ItemId effectiveItemId{ };
    QString itemLocation;

    QString getLocationText() const
    {
        auto result = itemLocation;
        if (message.fileName.isEmpty())
            return result;

        const auto fileTitle = FileDialog::getFileTitle(message.fileName);
        const auto locationAlreadyNamesFile =
            (result.compare(fileTitle, Qt::CaseInsensitive) == 0);
        if (!result.isEmpty() && !locationAlreadyNamesFile)
            result += QStringLiteral(" — ");
        if (!locationAlreadyNamesFile)
            result += fileTitle;
        if (message.line > 0)
            result += ":" + QString::number(message.line);
        return result;
    }
};

struct MessageSnapshot
{
    QList<MessageRow> rows;
    QHash<MessageId, int> rowByMessageId;
};

class MessageTableModel final : public QAbstractTableModel
{
public:
    explicit MessageTableModel(QObject *parent = nullptr)
        : QAbstractTableModel(parent)
        , mInfoIcon(QIcon::fromTheme("dialog-information"))
        , mWarningIcon(QIcon::fromTheme("dialog-warning"))
        , mErrorIcon(QIcon::fromTheme("dialog-error"))
        , mSnapshot(std::make_shared<MessageSnapshot>())
    {
    }

    int rowCount(const QModelIndex &parent = { }) const override
    {
        return (!parent.isValid() ? mSnapshot->rows.size() : 0);
    }

    int columnCount(const QModelIndex &parent = { }) const override
    {
        return (!parent.isValid() ? 2 : 0);
    }

    QVariant data(const QModelIndex &index, int role) const override
    {
        if (!index.isValid() || index.row() < 0
            || index.row() >= mSnapshot->rows.size())
            return { };

        const auto &row = mSnapshot->rows[index.row()];
        const auto &message = row.message;
        if (role == Qt::DisplayRole) {
            return (index.column() == 0 ? getMessageText(message)
                                        : row.getLocationText());
        }
        if (role == Qt::DecorationRole && index.column() == 0) {
            switch (getMessageSeverity(message)) {
            case MessageSeverity::Error:   return mErrorIcon;
            case MessageSeverity::Warning: return mWarningIcon;
            case MessageSeverity::Info:    return mInfoIcon;
            }
        }
        if (role == Qt::ToolTipRole) {
            const auto text = getMessageText(message);
            const auto location = row.getLocationText();
            return (location.isEmpty()
                    ? text
                    : text + QStringLiteral("\n") + location);
        }
        if (role == Qt::TextAlignmentRole && index.column() == 1)
            return static_cast<int>(Qt::AlignLeft | Qt::AlignVCenter);
        if (role == Qt::UserRole + 1)
            return row.effectiveItemId;
        if (role == Qt::UserRole + 2)
            return message.fileName;
        if (role == Qt::UserRole + 3)
            return message.line;
        if (role == Qt::UserRole + 4)
            return static_cast<int>(message.type);
        return { };
    }

    void setSnapshot(std::shared_ptr<const MessageSnapshot> snapshot)
    {
        const auto oldSize = mSnapshot->rows.size();
        const auto newSize = snapshot->rows.size();
        if (newSize > oldSize) {
            beginInsertRows({ }, oldSize, newSize - 1);
            mSnapshot = std::move(snapshot);
            endInsertRows();
        } else if (newSize < oldSize) {
            beginRemoveRows({ }, newSize, oldSize - 1);
            mSnapshot = std::move(snapshot);
            endRemoveRows();
        } else {
            mSnapshot = std::move(snapshot);
        }

        const auto changedSize = std::min(oldSize, newSize);
        if (changedSize > 0)
            Q_EMIT dataChanged(index(0, 0),
                index(changedSize - 1, columnCount() - 1));
    }

    const MessageRow *messageRow(const QModelIndex &index) const
    {
        if (!index.isValid() || index.row() < 0
            || index.row() >= mSnapshot->rows.size())
            return nullptr;
        return &mSnapshot->rows[index.row()];
    }

    int rowForMessageId(MessageId id) const
    {
        return mSnapshot->rowByMessageId.value(id, -1);
    }

    std::shared_ptr<const MessageSnapshot> snapshot() const
    {
        return mSnapshot;
    }

private:
    QIcon mInfoIcon;
    QIcon mWarningIcon;
    QIcon mErrorIcon;
    std::shared_ptr<const MessageSnapshot> mSnapshot;
};

namespace {
    constexpr auto MessageRefreshIntervalMs = 100;

    std::shared_ptr<const MessageSnapshot> buildMessageSnapshot(
        QList<Message> messages, const MessageLocationIndex &locations)
    {
        auto snapshot = std::make_shared<MessageSnapshot>();
        locations.sortMessages(messages);
        snapshot->rows.reserve(messages.size());
        for (auto &message : messages) {
            const auto effectiveItemId = locations.resolveItemId(message);

            auto row = MessageRow{
                .message = std::move(message),
                .effectiveItemId = effectiveItemId,
                .itemLocation = locations.getFullItemName(effectiveItemId),
            };
            snapshot->rows.append(std::move(row));
        }

        snapshot->rowByMessageId.reserve(snapshot->rows.size());
        for (auto row = 0; row < snapshot->rows.size(); ++row)
            snapshot->rowByMessageId.insert(snapshot->rows[row].message.id,
                row);
        return snapshot;
    }

    bool containsAddedMessages(const MessageSnapshot &previous,
        const MessageSnapshot &current)
    {
        for (const auto &row : current.rows)
            if (!previous.rowByMessageId.contains(row.message.id))
                return true;
        return false;
    }
} // namespace

MessageWindow::MessageWindow(QWidget *parent)
    : QTableView(parent)
    , mModel(std::make_unique<MessageTableModel>(this))
    , mExportButton(new QToolButton(this))
{
    setModel(mModel.get());
    connect(this, &MessageWindow::activated, this,
        &MessageWindow::handleItemActivated);

    mUpdateItemsTimer = new QTimer(this);
    connect(mUpdateItemsTimer, &QTimer::timeout, this,
        &MessageWindow::checkForUpdates);
    mUpdateItemsTimer->setInterval(MessageRefreshIntervalMs);
    mUpdateItemsTimer->start();

    verticalHeader()->setVisible(false);
    horizontalHeader()->setVisible(false);
    verticalHeader()->setSectionResizeMode(QHeaderView::Fixed);
    verticalHeader()->setDefaultSectionSize(fontMetrics().height() + 6);
    horizontalHeader()->setSectionResizeMode(0, QHeaderView::Stretch);
    horizontalHeader()->setSectionResizeMode(1, QHeaderView::Interactive);
    horizontalHeader()->resizeSection(1, 260);
    setEditTriggers(NoEditTriggers);
    setSelectionMode(SingleSelection);
    setSelectionBehavior(SelectRows);
    setVerticalScrollMode(ScrollPerPixel);
    setShowGrid(false);
    setAlternatingRowColors(true);
    setWordWrap(false);
    setTextElideMode(Qt::ElideRight);

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

MessageWindow::~MessageWindow() = default;

void MessageWindow::connectSessionModel()
{
    if (mSessionModelConnected)
        return;
    mSessionModelConnected = true;
    auto &model = Singletons::sessionModel();
    connect(&model, &QAbstractItemModel::modelReset, this,
        &MessageWindow::invalidateLocations);
    connect(&model, &QAbstractItemModel::rowsInserted, this,
        &MessageWindow::invalidateLocations);
    connect(&model, &QAbstractItemModel::rowsRemoved, this,
        &MessageWindow::invalidateLocations);
    connect(&model, &QAbstractItemModel::rowsMoved, this,
        &MessageWindow::invalidateLocations);
    connect(&model, &QAbstractItemModel::dataChanged, this,
        [this](const QModelIndex &topLeft, const QModelIndex &bottomRight,
            const QList<int> &roles) {
            if (roles.size() == 1 && roles.front() == Qt::ForegroundRole)
                return;
            const auto containsColumn = [&](int column) {
                return topLeft.column() <= column
                    && bottomRight.column() >= column;
            };
            if (containsColumn(SessionModel::Name)
                || containsColumn(SessionModel::FileName))
                invalidateLocations();
        });
}

void MessageWindow::invalidateLocations()
{
    ++mLocationRevision;
    mCachedLocations.reset();
}

void MessageWindow::checkForUpdates()
{
    connectSessionModel();
    if (mUpdateInProgress)
        return;

    auto messages = MessageList::getMessagesSnapshot();
    if (mAppliedMessagesRevision != messages.revision
        || mAppliedLocationRevision != mLocationRevision)
        startMessagesUpdate(std::move(messages));
}

void MessageWindow::startMessagesUpdate(MessagesSnapshot messages)
{
    mUpdateInProgress = true;
    const auto locationRevision = mLocationRevision;
    if (!mCachedLocations) {
        mCachedLocations =
            std::make_shared<MessageLocationIndex>(Singletons::sessionModel());
    }
    const auto locations = mCachedLocations;
    const auto previous = mModel->snapshot();
    const auto messagesRevision = messages.revision;
    const auto guard = QPointer<MessageWindow>(this);

    QThreadPool::globalInstance()->start(
        [guard, locations, locationRevision, previous, messagesRevision,
            messages = std::move(messages)]() mutable {
            const auto snapshot =
                buildMessageSnapshot(std::move(messages), *locations);
            const auto added = containsAddedMessages(*previous, *snapshot);

            QMetaObject::invokeMethod(
                qApp,
                [guard, snapshot, messagesRevision, locationRevision, added]() {
                    if (guard)
                        guard->applySnapshot(snapshot, messagesRevision,
                            locationRevision, added);
                },
                Qt::QueuedConnection);
        });
}

void MessageWindow::applySnapshot(
    std::shared_ptr<const MessageSnapshot> snapshot,
    qulonglong messagesRevision, qulonglong locationRevision,
    bool messagesAdded)
{
    mUpdateInProgress = false;
    if (locationRevision != mLocationRevision)
        return;

    auto selectedId = MessageId{ };
    if (const auto row = mModel->messageRow(currentIndex()))
        selectedId = row->message.id;
    auto topId = MessageId{ };
    if (const auto row = mModel->messageRow(indexAt({ 0, 0 })))
        topId = row->message.id;

    mModel->setSnapshot(std::move(snapshot));
    if (topId) {
        const auto row = mModel->rowForMessageId(topId);
        if (row >= 0)
            scrollTo(mModel->index(row, 0), PositionAtTop);
    }
    if (selectedId) {
        const auto row = mModel->rowForMessageId(selectedId);
        if (row >= 0)
            setCurrentIndex(mModel->index(row, 0));
    }

    mAppliedMessagesRevision = messagesRevision;
    mAppliedLocationRevision = locationRevision;
    if (messagesAdded)
        Q_EMIT this->messagesAdded();
}

void MessageWindow::handleItemActivated(const QModelIndex &index)
{
    const auto row = mModel->messageRow(index);
    if (!row)
        return;
    const auto &message = row->message;
    auto line = message.line;

    // Some backends only report a line number in the diagnostic text.
    static const auto regex = QRegularExpression("in line (\\d+)",
        QRegularExpression::MultilineOption);
    if (auto match = regex.match(getMessageText(message)); match.hasMatch())
        line = match.capturedView(1).toInt();

    Q_EMIT messageActivated(row->effectiveItemId, message.fileName, line, -1);
}

void MessageWindow::exportMessages()
{
    const auto snapshot = mModel->snapshot();
    auto text = QString();
    auto out = QTextStream(&text);
    for (const auto &row : snapshot->rows)
        out << getMessageText(row.message) << "\t" << row.getLocationText()
            << "\n";

    auto editor =
        Singletons::editorManager().getSourceEditor(mLastExportFileName);
    if (!editor)
        mLastExportFileName =
            FileDialog::generateNextUntitledFileName("Messages");
    editor = Singletons::editorManager().openSourceEditor(mLastExportFileName);
    if (editor)
        editor->replace(text);
}
