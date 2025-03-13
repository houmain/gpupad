#include "ConsoleScriptObject.h"

ConsoleScriptObject::ConsoleScriptObject(QObject *parent) : QObject(parent) { }

void ConsoleScriptObject::setMessages(MessagePtrSet *messages,
    const QString &fileName)
{
    mMessages = messages;
    mFileName = fileName;
    mItemId = {};
}

void ConsoleScriptObject::setMessages(MessagePtrSet *messages, ItemId itemId)
{
    mMessages = messages;
    mItemId = itemId;
    mFileName.clear();
}

void ConsoleScriptObject::output(QString message, int level)
{
    const auto messageType = level == 2 ? MessageType::ScriptError
        : level == 1                    ? MessageType::ScriptWarning
                                        : MessageType::ScriptMessage;

    (*mMessages) += (!mFileName.isEmpty()
            ? MessageList::insert(mFileName, 0, messageType, message, false)
            : MessageList::insert(mItemId, messageType, message, false));
}
