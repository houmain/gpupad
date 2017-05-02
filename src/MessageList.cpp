#include "MessageList.h"
#include "Singletons.h"
#include "MessageWindow.h"

MessageList::MessageList(MessageList &&rhs)
    : mMessages(std::move(rhs.mMessages))
    , mItemId(rhs.mItemId)
    , mFileName(rhs.mFileName)
{
}

MessageList &MessageList::operator=(MessageList &&rhs)
{
    auto tmp = std::move(rhs);
    std::swap(mMessages, tmp.mMessages);
    std::swap(mItemId, tmp.mItemId);
    std::swap(mFileName, tmp.mFileName);
    return *this;
}

MessageList::~MessageList()
{
    clear();
}

void MessageList::clear()
{
    foreach (Message *message, mMessages) {
        Singletons::messageWindow().removeMessage(message);
        delete message;
    }
    mMessages.clear();
}

void MessageList::setContext(ItemId itemId)
{
    mItemId = itemId;
    mFileName = "";
}

void MessageList::setContext(QString fileName)
{
    mFileName = fileName;
    mItemId = 0;
}

void MessageList::insert(MessageType type, QString text, int line, int column)
{
    // merge lines with identical location
    if (!mMessages.empty()) {
        auto& prev = *mMessages.back();
        if (prev.type == type &&
            prev.fileName == mFileName &&
            prev.itemId == mItemId &&
            prev.line == line) {
            prev.text += "\n" + text;
            return;
        }
    }

    auto message = new Message{ type, text, mItemId, mFileName, line, column };
    mMessages.append(message);
    Singletons::messageWindow().insertMessage(message);
}
