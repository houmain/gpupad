#pragma once

#include <QSet>
#include <QSharedPointer>
#include <QString>
#include <chrono>

using ItemId = int;
using MessageId = qulonglong;
using MessagePtr = QSharedPointer<const struct Message>;
using MessagePtrSet = QSet<MessagePtr>;

enum MessageType {
    OpenGLVersionNotAvailable,
    LoadingFileFailed,
    UnsupportedShaderType,
    UnsupportedTextureFormat,
    CreatingFramebufferFailed,
    CreatingTextureFailed,
    UploadingImageFailed,
    DownloadingImageFailed,
    UniformNotSet,
    BufferNotSet,
    SamplerNotSet,
    ImageNotSet,
    AttributeNotSet,
    ShaderInfo,
    ShaderWarning,
    ShaderError,
    TotalDuration,
    CallDuration,
    CallFailed,
    ClearingTextureFailed,
    CopyingTextureFailed,
    ScriptError,
    ScriptWarning,
    ScriptMessage,
    ProgramNotAssigned,
    TargetNotAssigned,
    TextureNotAssigned,
    BufferNotAssigned,
    SwappingTexturesFailed,
    SwappingBuffersFailed,
    InvalidAttribute,
    InvalidSubroutine,
    ImageFormatNotBindable,
    UniformComponentMismatch,
    InvalidIncludeDirective,
    IncludableNotFound,
    RecursiveInclude,
    TooManyPrintfCalls,
    RenderingFailed,
    MoreThanOneDepthStencilAttachment,
    IncompatibleBindings,
    CreatingPipelineFailed,
    OpenGLRendererRequiresGLSL,
    SubroutinesNotAvailableInVulkan,
    CantSampleAttachment,
    SampleCountMismatch,
    MaxSampleCountExceeded,
    MaxPushConstantSizeExceeded,
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

namespace MessageList {
    MessagePtr insert(QString fileName, int line, MessageType type,
        QString text = "", bool deduplicate = true);
    MessagePtr insert(ItemId itemId, MessageType type, QString text = "",
        bool deduplicate = true);
    QList<MessagePtr> messages();
}; // namespace MessageList

QString formatDuration(const std::chrono::duration<double> &duration);
