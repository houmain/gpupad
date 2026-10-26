#pragma once

#include <QString>
#include <chrono>
#include <memory>

using ItemId = int;
using MessageId = qulonglong;

enum class MessageType {
    None,
    NotImplemented,
    OpenGLVersionNotAvailable,
    VulkanNotAvailable,
    Direct3DNotAvailable,
    LoadingFileFailed,
    ConvertingFileFailed,
    UnsupportedShaderType,
    ProgramHasNoShader,
    UnsupportedTextureFormat,
    CreatingTargetFailed,
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
    InvalidShaderTypeForCall,
    ImageFormatNotBindable,
    CountExceeded,
    UniformComponentMismatch,
    InvalidIncludeDirective,
    IncludableNotFound,
    RecursiveInclude,
    TooManyPrintfCalls,
    RenderingFailed,
    MoreThanOneDepthStencilAttachment,
    TooManyColorAttachments,
    IncompatibleBindings,
    CreatingPipelineFailed,
    OpenGLRendererRequiresGLSL,
    OpenGLRequiresCombinedTextureSamplers,
    CantSampleAttachment,
    SampleCountMismatch,
    MaxSampleCountExceeded,
    MaxPushConstantSizeExceeded,
    MaxVariableBindGroupEntriesExceeded,
    OnlyLastBindingMayBeUnsizedArray,
    RayTracingNotAvailable,
    MeshShadersNotAvailable,
    SpirvCrossError,
    ShaderCompilerNotAvailable,
    InvalidCommandlineArguments,
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

using MessagePtr = std::shared_ptr<const Message>;

enum class MessageSeverity {
    Error,
    Warning,
    Info,
};

QString formatDuration(const std::chrono::duration<double> &duration);
MessageSeverity getMessageSeverity(const Message &message);
QString getMessageSeverityText(const Message &message);
QString getMessageText(const Message &message);
