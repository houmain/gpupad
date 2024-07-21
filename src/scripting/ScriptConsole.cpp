#include "ScriptConsole.h"

ScriptConsole::ScriptConsole(QObject *parent) : QObject(parent) { }

void ScriptConsole::setMessages(MessagePtrSet *messages,
    const QString &fileName)
{
    mMessages = messages;
    mFileName = fileName;
    mItemId = {};
}

void ScriptConsole::setMessages(MessagePtrSet *messages, ItemId itemId)
{
    mMessages = messages;
    mItemId = itemId;
    mFileName.clear();
}

void ScriptConsole::output(QString message, int level)
{
    const auto messageType = level == 2 ? MessageType::ScriptError
        : level == 1                    ? MessageType::ScriptWarning
                                        : MessageType::ScriptMessage;

    (*mMessages) += (!mFileName.isEmpty()
            ? MessageList::insert(mFileName, 0, messageType, message)
            : MessageList::insert(mItemId, messageType, message));
}
