#include "ConsoleScriptObject.h"
#include <QRegularExpression>
#include <utility>

ConsoleScriptObject::ConsoleScriptObject(MessageList *messages,
    QObject *parent)
    : QObject(parent)
    , mMessages(*messages)
{
}

std::shared_ptr<void> ConsoleScriptObject::pushState()
{
    return std::shared_ptr<void>(nullptr,
        [this, fileName = mFileName, itemId = mItemId](void *) {
            mFileName = fileName;
            mItemId = itemId;
        });
}

std::shared_ptr<void> ConsoleScriptObject::setLocation(ItemId itemId,
    const QString &fileName)
{
    auto state = pushState();
    mFileName = fileName;
    mItemId = itemId;
    return state;
}

std::shared_ptr<void> ConsoleScriptObject::setItemId(ItemId itemId)
{
    return setLocation(itemId, {});
}

void ConsoleScriptObject::output(QString message, int level)
{
    static const auto emptyObjectName = QRegularExpression(
        R"("objectName":\s*"",\s*)", QRegularExpression::MultilineOption);
    if (message.contains(QStringLiteral("\"objectName\"")))
        message.replace(emptyObjectName, "");

    const auto messageType = (level == 2 ? MessageType::ScriptError
            : level == 1                 ? MessageType::ScriptWarning
                                         : MessageType::ScriptMessage);
    mMessages.insert(mItemId, messageType, std::move(message), mFileName, 0);
}
