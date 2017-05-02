#ifndef MESSAGELIST_H
#define MESSAGELIST_H

#include <QList>

using ItemId = int;

enum MessageType
{
    Error,
    Warning,
    Info,
    OpenGL33NotSupported,
    LoadingFileFailed,
    UnsupportedShaderType,
};

struct Message
{
    MessageType type;
    QString text;

    ItemId itemId;
    QString fileName;
    int line;
    int column;
};

class MessageList
{
public:
    MessageList() = default;
    MessageList(MessageList &&rhs);
    MessageList &operator=(MessageList &&rhs);
    ~MessageList();

    bool empty() const { return mMessages.empty(); }
    void clear();
    void setContext(ItemId itemId);
    void setContext(QString fileName);
    void insert(MessageType type, QString text = "", int line = -1, int column = -1);

private:
    QList<Message*> mMessages;
    ItemId mItemId{ };
    QString mFileName{ };
};

#endif // MESSAGELIST_H
