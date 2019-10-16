#ifndef MESSAGELIST_H
#define MESSAGELIST_H

#include <QSharedPointer>
#include <QString>
#include <QSet>

using ItemId = int;

enum MessageType
{
    OpenGLVersionNotAvailable,
    LoadingFileFailed,
    UnsupportedShaderType,
    CreatingFramebufferFailed,
    UploadingImageFailed,
    DownloadingImageFailed,
    UnformNotSet,
    BlockNotSet,
    AttributeNotSet,
    ShaderInfo,
    ShaderWarning,
    ShaderError,
    CallDuration,
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
    qulonglong id;
    MessageType type;
    QString text;
    ItemId itemId;
    QString fileName;
    int line;
};

using MessagePtr = QSharedPointer<const Message>;
using MessagePtrSet = QSet<MessagePtr>;

namespace MessageList
{
    MessagePtr insert(QString fileName, int line, MessageType type,
        QString text = "", bool deduplicate = true);
    MessagePtr insert(ItemId itemId, MessageType type,
        QString text = "", bool deduplicate = true);
    QList<MessagePtr> messages();
};

#endif // MESSAGELIST_H
