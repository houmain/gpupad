#include "MessageList.h"
#include "FileDialog.h"
#include <QCoreApplication>
#include <QMutex>
#include <QHash>

size_t qHash(const Message &message, size_t seed)
{
    return qHashMulti(seed, static_cast<int>(message.type), message.text,
        message.itemId, message.fileName, message.line);
}

bool operator==(const Message &a, const Message &b)
{
    const auto tie = [](const auto &v) {
        return std::tie(v.type, v.text, v.itemId, v.fileName, v.line);
    };
    return tie(a) == tie(b);
}

namespace {
    QMutex gMessagesMutex;
    QHash<Message, QWeakPointer<const Message>> gUniqueMessages;
    QList<QWeakPointer<const Message>> gMessages;
    qulonglong gNextMessageId = 1;
} // namespace

MessagePtr MessagePtrSet::makeMessage(ItemId itemId, MessageType type,
    QString text, QString fileName, int line, bool deduplicate)
{
    QMutexLocker lock(&gMessagesMutex);
    auto message = Message{ 0, type, text, itemId, fileName, line };
    if (deduplicate)
        if (auto messagePtr = gUniqueMessages.value(message).lock())
            return messagePtr;
    message.id = gNextMessageId++;
    const auto messagePtr = MessagePtr(new Message{ message });
    if (deduplicate) {
        gUniqueMessages.insert(message, messagePtr);
    } else {
        gMessages.append(messagePtr);
    }
    return messagePtr;
}

QList<MessagePtr> MessagePtrSet::getAllMessages()
{
    QMutexLocker lock(&gMessagesMutex);
    auto result = QList<MessagePtr>();
    QMutableHashIterator<Message, QWeakPointer<const Message>> it(
        gUniqueMessages);
    while (it.hasNext()) {
        if (MessagePtr message = it.next().value().lock()) {
            result += message;
        } else {
            it.remove();
        }
    }
    QMutableListIterator<QWeakPointer<const Message>> it2(gMessages);
    while (it2.hasNext()) {
        if (MessagePtr message = it2.next().lock()) {
            result += message;
        } else {
            it2.remove();
        }
    }
    return result;
}

QString formatDuration(const std::chrono::duration<double> &duration)
{
    auto toString = [](double v) { return QString::number(v, 'f', 2); };

    auto seconds = duration.count();
    if (duration > std::chrono::seconds(1))
        return toString(seconds) + "s";

    if (duration > std::chrono::milliseconds(1))
        return toString(seconds * 1000) + "ms";

    if (duration > std::chrono::microseconds(1))
        return toString(seconds * 1000000ull) + QChar(181) + "s";

    return toString(seconds * 1000000000ull) + "ns";
}

MessageSeverity getMessageSeverity(const Message &message)
{
    using enum MessageType;
    switch (message.type) {
    case UniformNotSet:
    case UniformComponentMismatch:
    case ShaderWarning:
    case ScriptWarning:
    case TooManyPrintfCalls:       return MessageSeverity::Warning;

    case ShaderInfo:
    case ScriptMessage:
    case CallDuration:
    case TotalDuration: return MessageSeverity::Info;

    default: return MessageSeverity::Error;
    }
}

QString getMessageText(const Message &message)
{
#define tr qApp->tr
    using enum MessageType;
    switch (message.type) {
    case None:
    case ShaderInfo:
    case ShaderWarning:
    case ShaderError:
    case ScriptError:
    case ScriptWarning:
    case ScriptMessage: return message.text;

    case NotImplemented:
        return tr("Not implemented")
            + (!message.text.isEmpty()
                    ? QStringLiteral(": %1").arg(message.text)
                    : "");
    case SpirvCrossError: return tr("Spirv-Cross Error: %1").arg(message.text);
    case OpenGLVersionNotAvailable:
        return tr("The required OpenGL version %1 is not available")
            .arg(message.text);
    case VulkanNotAvailable:
        return tr("Vulkan is not available")
            + (!message.text.isEmpty()
                    ? QStringLiteral(" (%1)").arg(message.text)
                    : "");
    case Direct3DNotAvailable:
        return tr("Direct3D is not available")
            + (!message.text.isEmpty()
                    ? QStringLiteral(" (%1)").arg(message.text)
                    : "");
    case LoadingFileFailed:
        if (message.text.isEmpty())
            return tr("No file set");
        return tr("Loading file '%1' failed")
            .arg(FileDialog::getFileTitle(message.text));
    case ConvertingFileFailed:
        return tr("Converting file '%1' failed")
            .arg(FileDialog::getFileTitle(message.text));
    case UnsupportedShaderType:    return tr("Unsupported shader type");
    case ProgramHasNoShader:       return tr("Program has no shader");
    case UnsupportedTextureFormat: return tr("Unsupported texture format");
    case CreatingFramebufferFailed:
        return tr("Creating framebuffer failed %1").arg(message.text);
    case CreatingTextureFailed:  return tr("Creating texture failed");
    case UploadingImageFailed:   return tr("Uploading image failed");
    case DownloadingImageFailed: return tr("Downloading image failed");
    case UniformNotSet:          return tr("Uniform '%1' not set").arg(message.text);
    case BufferNotSet:           return tr("Buffer '%1' not set").arg(message.text);
    case SamplerNotSet:          return tr("Sampler '%1' not set").arg(message.text);
    case CantSampleAttachment:
        return tr("Cannot sample attachment '%1'").arg(message.text);
    case ImageNotSet: return tr("Image '%1' not set").arg(message.text);
    case SubroutineNotSet:
        return tr("Subroutine '%1' not set").arg(message.text);
    case AttributeNotSet:           return tr("Attribute '%1' not set").arg(message.text);
    case CallDuration:              return tr("Call took %1").arg(message.text);
    case TotalDuration:             return tr("Total duration %1").arg(message.text);
    case CallFailed:                return tr("Call failed: %1").arg(message.text);
    case ClearingTextureFailed:     return tr("Clearing texture failed");
    case CopyingTextureFailed:      return tr("Copying texture failed");
    case SwappingTexturesFailed:    return tr("Swapping textures failed");
    case SwappingBuffersFailed:     return tr("Swapping buffers failed");
    case ProgramNotAssigned:        return tr("No program set");
    case TargetNotAssigned:         return tr("No target set");
    case IndexBufferNotAssigned:    return tr("No index buffer set");
    case IndirectBufferNotAssigned: return tr("No indirect buffer set");
    case AccelerationStructureNotAssigned:
        return tr("No acceleration structure set");
    case TextureNotAssigned: return tr("No texture set");
    case BufferNotAssigned:  return tr("No buffer set");
    case InvalidSubroutine:
        return tr("Invalid subroutine '%1'").arg(message.text);
    case ImageFormatNotBindable: return tr("Image format not bindable");
    case UniformComponentMismatch:
        return tr("Uniform component mismatch %1").arg(message.text);
    case InvalidIncludeDirective: return tr("Invalid #include directive");
    case IncludableNotFound:
        return tr("Includable shader '%1' not found").arg(message.text);
    case RecursiveInclude:
        return tr("Recursive #include '%1'").arg(message.text);
    case InvalidAttribute: return tr("Invalid stream attribute");
    case InvalidIndexType:
        return tr("Invalid index type (%1)").arg(message.text);
    case InvalidIndirectStride:
        return tr("Invalid indirect stride (%1)").arg(message.text);
    case InvalidGeometryStride:
        return tr("Invalid geometry stride (%1)").arg(message.text);
    case InvalidShaderTypeForCall: return tr("Invalid shader type for call");
    case CountExceeded:
        return tr("Maximum count exceeded (%1)").arg(message.text);
    case TooManyPrintfCalls: return tr("Too many printf calls");
    case RenderingFailed:    return tr("Rendering failed: %1").arg(message.text);
    case MoreThanOneDepthStencilAttachment:
        return tr("Only a single depth or stencil attachment is supported");
    case TooManyColorAttachments: return tr("Too many color attachments");
    case IncompatibleBindings:
        return tr("Incompatible assignment to the same set/binding %1")
            .arg(message.text);
    case CreatingPipelineFailed: return tr("Creating pipeline failed");
    case OpenGLRendererRequiresGLSL:
        return tr("The OpenGL driver can only compile GLSL shaders");
    case OpenGLRequiresCombinedTextureSamplers:
        return tr("OpenGL requires combined texture/samplers");
    case SampleCountMismatch:
        return tr("Sample count of attachments does not match");
    case MaxSampleCountExceeded:
        return tr("Maximum sample count exceeded (%1)").arg(message.text);
    case MaxPushConstantSizeExceeded:
        return tr("Maximum push constant size exceeded (%1)").arg(message.text);
    case MaxVariableBindGroupEntriesExceeded:
        return tr("Maximum variable binding group entries exceeded (%1)")
            .arg(message.text);
    case OnlyLastBindingMayBeUnsizedArray:
        return tr("Only the last binding may be an unsized array (%1)")
            .arg(message.text);
    case RayTracingNotAvailable:  return tr("Raytracing not available");
    case MeshShadersNotAvailable: return tr("Mesh Shaders not available");
    }
    return message.text;
#undef tr
}
