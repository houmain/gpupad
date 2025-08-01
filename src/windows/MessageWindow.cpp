#include "MessageWindow.h"
#include "FileDialog.h"
#include "MessageList.h"
#include "Singletons.h"
#include "session/SessionModel.h"
#include <QHeaderView>
#include <QRegularExpression>
#include <QStandardItemModel>
#include <QTimer>

MessageWindow::MessageWindow(QWidget *parent) : QTableWidget(parent)
{
    connect(this, &MessageWindow::itemActivated, this,
        &MessageWindow::handleItemActivated);

    mUpdateItemsTimer = new QTimer(this);
    connect(mUpdateItemsTimer, &QTimer::timeout, this,
        &MessageWindow::updateMessages);
    mUpdateItemsTimer->setSingleShot(true);
    mUpdateItemsTimer->start();

    setColumnCount(2);
    verticalHeader()->setVisible(true);
    horizontalHeader()->setVisible(false);
    verticalHeader()->setSectionResizeMode(QHeaderView::Interactive);
    verticalHeader()->setDefaultSectionSize(24);
    horizontalHeader()->setSectionResizeMode(0, QHeaderView::Stretch);
    horizontalHeader()->setSectionResizeMode(1, QHeaderView::ResizeToContents);
    setEditTriggers(NoEditTriggers);
    setSelectionMode(SingleSelection);
    setSelectionBehavior(SelectRows);
    setVerticalScrollMode(ScrollPerPixel);
    setShowGrid(false);
    setAlternatingRowColors(true);
    setWordWrap(true);

    mInfoIcon = QIcon::fromTheme("dialog-information");
    mWarningIcon = QIcon::fromTheme("dialog-warning");
    mErrorIcon = QIcon::fromTheme("dialog-error");
}

void MessageWindow::updateMessages()
{
    auto added = false;
    auto messages = MessageList::messages();
    auto messageIds = QSet<MessageId>();
    for (auto it = messages.begin(); it != messages.end();) {
        const auto &message = **it;
        added |= addMessageOnce(message);
        messageIds += message.id;
        ++it;
    }
    removeMessagesExcept(messageIds);

    if (added)
        Q_EMIT messagesAdded();

    mUpdateItemsTimer->start(50);
}

QIcon MessageWindow::getMessageIcon(const Message &message) const
{
    using enum MessageType;
    switch (message.type) {
    case UniformNotSet:
    case UniformComponentMismatch:
    case ShaderWarning:
    case ScriptWarning:
    case TooManyPrintfCalls:       return mWarningIcon;

    case ShaderInfo:
    case ScriptMessage:
    case CallDuration:
    case TotalDuration: return mInfoIcon;

    default: return mErrorIcon;
    }
}

QString MessageWindow::getMessageText(const Message &message) const
{
    using enum MessageType;
    switch (message.type) {
    case None:
    case ShaderInfo:
    case ShaderWarning:
    case ShaderError:
    case ScriptError:
    case ScriptWarning:
    case ScriptMessage: return message.text;

    case OpenGLVersionNotAvailable:
        return tr("The required OpenGL version %1 is not available")
            .arg(message.text);
    case VulkanNotAvailable:
        return tr("Vulkan is not available")
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
    case IncompatibleBindings:
        return tr("Incompatible assignment to the same set/binding %1")
            .arg(message.text);
    case CreatingPipelineFailed: return tr("Creating pipeline failed");
    case OpenGLRendererRequiresGLSL:
        return tr("The OpenGL driver can only compile GLSL shaders");
    case OpenGLRequiresCombinedTextureSamplers:
        return tr("OpenGL requires combined texture/samplers");
    case SubroutinesNotAvailableInVulkan:
        return tr("Subroutines not available in Vulkan");
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
        return tr("Only the last binding may be an unsized array (%1)").arg(message.text);
    case TextureBuffersNotAvailable:
        return tr("Texture buffers not available in Vulkan yet");
    case RayTracingNotAvailable:  return tr("Raytracing not available");
    case MeshShadersNotAvailable: return tr("Mesh Shaders not available");
    }
    return message.text;
}

QString MessageWindow::getLocationText(const Message &message) const
{
    auto locationText = QString();
    if (message.itemId) {
        locationText +=
            Singletons::sessionModel().getFullItemName(message.itemId);
    } else if (!message.fileName.isEmpty()) {
        locationText = FileDialog::getFileTitle(message.fileName);
        if (message.line > 0)
            locationText += ":" + QString::number(message.line);
    }
    return locationText;
}

void MessageWindow::removeMessagesExcept(const QSet<MessageId> &messageIds)
{
    for (auto row = 0; row < mMessageIds.size();)
        if (!messageIds.contains(mMessageIds[row])) {
            removeRow(row);
            mMessageIds.removeAt(row);
        } else {
            ++row;
        }
}

bool MessageWindow::addMessageOnce(const Message &message)
{
    auto it =
        std::lower_bound(mMessageIds.begin(), mMessageIds.end(), message.id);
    if (it != mMessageIds.end() && *it == message.id)
        return false;
    const auto row = std::distance(mMessageIds.begin(), it);
    mMessageIds.insert(it, message.id);

    auto messageIcon = getMessageIcon(message);
    auto messageText = getMessageText(message);
    auto messageItem = new QTableWidgetItem(messageIcon, messageText);
    messageItem->setData(Qt::UserRole + 1, message.itemId);
    messageItem->setData(Qt::UserRole + 2, message.fileName);
    messageItem->setData(Qt::UserRole + 3, message.line);
    messageItem->setData(Qt::UserRole + 4, static_cast<int>(message.type));

    auto locationText = getLocationText(message);
    auto locationItem = new QTableWidgetItem(locationText);
    locationItem->setTextAlignment(Qt::AlignLeft | Qt::AlignTop);

    insertRow(row);
    setItem(row, 0, messageItem);
    setItem(row, 1, locationItem);
    return true;
}

void MessageWindow::handleItemActivated(QTableWidgetItem *messageItem)
{
    auto itemId = messageItem->data(Qt::UserRole + 1).toInt();
    auto fileName = messageItem->data(Qt::UserRole + 2).toString();
    auto line = messageItem->data(Qt::UserRole + 3).toInt();

    // parse line number from text
    static const auto regex = QRegularExpression("in line (\\d+)",
        QRegularExpression::MultilineOption);
    if (auto match = regex.match(messageItem->text()); match.hasMatch())
        line = match.capturedView(1).toInt();

    Q_EMIT messageActivated(itemId, fileName, line, -1);
}
