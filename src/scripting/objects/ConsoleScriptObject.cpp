#include "ConsoleScriptObject.h"
#include <QRegularExpression>

ConsoleScriptObject::ConsoleScriptObject(MessagePtrSet *messages,
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

std::shared_ptr<void> ConsoleScriptObject::setFileName(const QString &fileName)
{
    auto state = pushState();
    mFileName = fileName;
    mItemId = {};
    return state;
}

std::shared_ptr<void> ConsoleScriptObject::setItemId(ItemId itemId)
{
    auto state = pushState();
    mItemId = itemId;
    mFileName.clear();
    return state;
}

void ConsoleScriptObject::output(QString message, int level)
{
    static const auto emptyObjectName = QRegularExpression(
        R"("objectName":\s*"",\s*)", QRegularExpression::MultilineOption);
    message = message.replace(emptyObjectName, "");

    const auto messageType = (level == 2 ? MessageType::ScriptError
            : level == 1                 ? MessageType::ScriptWarning
                                         : MessageType::ScriptMessage);
    if (!mFileName.isEmpty()) {
        mMessages.insert(mFileName, 0, messageType, message, false);
    } else {
        mMessages.insert(mItemId, messageType, message, false);
    }
}
