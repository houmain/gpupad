#pragma once

#include <QSet>
#include <QSharedPointer>
#include <QString>
#include <chrono>

using ItemId = int;
using MessageId = qulonglong;
using MessagePtr = QSharedPointer<const struct Message>;
using MessagePtrSet = QSet<MessagePtr>;

enum class MessageType {
    None,
    OpenGLVersionNotAvailable,
    VulkanNotAvailable,
    LoadingFileFailed,
    ConvertingFileFailed,
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
    SubroutineNotSet,
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
    IndexBufferNotAssigned,
    IndirectBufferNotAssigned,
    AccelerationStructureNotAssigned,
    TextureNotAssigned,
    BufferNotAssigned,
    SwappingTexturesFailed,
    SwappingBuffersFailed,
    InvalidAttribute,
    InvalidSubroutine,
    InvalidIndexType,
    InvalidIndirectStride,
    InvalidGeometryStride,
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
    OpenGLRequiresCombinedTextureSamplers,
    CantSampleAttachment,
    SampleCountMismatch,
    MaxSampleCountExceeded,
    MaxPushConstantSizeExceeded,
    TextureBuffersNotAvailable,
    RayTracingNotAvailable,
    MeshShadersNotAvailable,
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
