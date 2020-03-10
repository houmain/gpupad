#ifndef MESSAGELIST_H
#define MESSAGELIST_H

#include <QSharedPointer>
#include <QString>
#include <QSet>

using ItemId = int;
using MessageId = qulonglong;
using MessagePtr = QSharedPointer<const struct Message>;
using MessagePtrSet = QSet<MessagePtr>;

enum MessageType
{
    OpenGLVersionNotAvailable,
    LoadingFileFailed,
    UnsupportedShaderType,
    CreatingFramebufferFailed,
    UploadingImageFailed,
    DownloadingImageFailed,
    UnformNotSet,
    BufferNotSet,
    AttributeNotSet,
    ShaderInfo,
    ShaderWarning,
    ShaderError,
    CallDuration,
    CallFailed,
    ScriptError,
    ScriptMessage,
    ProgramNotAssigned,
    TextureNotAssigned,
    BufferNotAssigned,
    InvalidSubroutine,
    ImageFormatNotBindable,
    UniformComponentMismatch,
};

struct Message
{
    MessageId id;
    MessageType type;
    QString text;
    ItemId itemId;
    QString fileName;
    int line;
};

namespace MessageList
{
    MessagePtr insert(QString fileName, int line, MessageType type,
        QString text = "", bool deduplicate = true);
    MessagePtr insert(ItemId itemId, MessageType type,
        QString text = "", bool deduplicate = true);
    QList<MessagePtr> messages();
};

#endif // MESSAGELIST_H
