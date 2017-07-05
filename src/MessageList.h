#ifndef MESSAGELIST_H
#define MESSAGELIST_H

#include <QSharedPointer>
#include <QString>
#include <QSet>
#include <QMutex>

using ItemId = int;

enum MessageType
{
    Error,
    Warning,
    Info,
    OpenGL33NotSupported,
    LoadingFileFailed,
    UnsupportedShaderType,
    CreatingFramebufferFailed
};

struct Message
{
    MessageType type;
    QString text;
    ItemId itemId;
    QString fileName;
    int line;
};

using MessagePtr = QSharedPointer<const Message>;
using MessagePtrSet = QSet<MessagePtr>;

class MessageList
{
public:
    MessagePtr insert(QString fileName, int line,
        MessageType type, QString text = "");
    MessagePtr insert(ItemId itemId,
        MessageType type, QString text = "");

    MessagePtrSet messages() const;

private:
    mutable QMutex mMessagesMutex;
    mutable QList<QWeakPointer<const Message>> mMessages;
};

#endif // MESSAGELIST_H
