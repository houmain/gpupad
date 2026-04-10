#pragma once

#include <QSet>
#include <QSharedPointer>
#include <chrono>

using ItemId = int;
using MessageId = qulonglong;
using MessagePtr = QSharedPointer<const struct Message>;

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

enum class MessageSeverity {
    Error,
    Warning,
    Info,
};

class MessagePtrSet : QSet<MessagePtr>
{
public:
    static MessagePtr makeMessage(ItemId itemId, MessageType type, QString text,
        QString fileName, int line, bool deduplicate = true);
    static QList<MessagePtr> getAllMessages();

    using QSet::clear;
    using QSet::insert;
    using QSet::size;

    MessagePtrSet &operator+=(const MessagePtr &message)
    {
        QSet::operator+=(message);
        return *this;
    }

    MessagePtrSet &operator+=(const MessagePtrSet &other)
    {
        QSet::operator+=(other);
        return *this;
    }

    void insert(QString fileName, int line, MessageType type, QString text = "",
        bool deduplicate = true)
    {
        insert(makeMessage(0, type, text, fileName, line, deduplicate));
    }

    void insert(ItemId itemId, MessageType type, QString text = "",
        bool deduplicate = true)
    {
        insert(makeMessage(itemId, type, text, "", 0, deduplicate));
    }

    void insert(ItemId itemId, MessageType type, QString text, QString fileName,
        int line, bool deduplicate = true)
    {
        insert(makeMessage(itemId, type, text, fileName, line, deduplicate));
    }
};

QString formatDuration(const std::chrono::duration<double> &duration);
MessageSeverity getMessageSeverity(const Message &message);
QString getMessageText(const Message &message);
