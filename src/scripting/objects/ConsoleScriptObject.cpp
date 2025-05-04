#include "ConsoleScriptObject.h"

ConsoleScriptObject::ConsoleScriptObject(MessagePtrSet *messages, QObject *parent)
    : QObject(parent)
    , mMessages(*messages)
{
}

void ConsoleScriptObject::setFileName(const QString &fileName)
{
    mFileName = fileName;
    mItemId = {};
}

void ConsoleScriptObject::setItemId(ItemId itemId)
{
    mItemId = itemId;
    mFileName.clear();
}

void ConsoleScriptObject::output(QString message, int level)
{
    const auto messageType = (level == 2 ? MessageType::ScriptError
            : level == 1                 ? MessageType::ScriptWarning
                                         : MessageType::ScriptMessage);

    mMessages += (!mFileName.isEmpty()
            ? MessageList::insert(mFileName, 0, messageType, message, false)
            : MessageList::insert(mItemId, messageType, message, false));
}
