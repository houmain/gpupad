#include "VKTarget.h"

VKTarget::VKTarget(const Target &target)
    : mItemId(target.id)
    , mFrontFace(target.frontFace)
    , mCullMode(target.cullMode)
    , mPolygonMode(target.polygonMode)
    , mLogicOperation(target.logicOperation)
    , mBlendConstant(target.blendConstant)
    , mDefaultWidth(target.defaultWidth)
    , mDefaultHeight(target.defaultHeight)
    , mDefaultLayers(target.defaultLayers)
    , mDefaultSamples(target.defaultSamples)
{
    mUsedItems += target.id;

    auto attachmentIndex = 0;
    for (const auto &item : target.items) {
        if (auto attachment = castItem<Attachment>(item)) {
            static_cast<Attachment&>(mAttachments[attachmentIndex]) = *attachment;
            mUsedItems += attachment->id;
        }
        attachmentIndex++;
    }
}

void VKTarget::setTexture(int index, VKTexture *texture)
{
    mAttachments[index].texture = texture;
}

KDGpu::RenderPassCommandRecorderOptions VKTarget::prepare(VKContext &context)
{
    auto renderPassOptions = KDGpu::RenderPassCommandRecorderOptions{ };

    for (auto &attachment : mAttachments)
        if (auto texture = attachment.texture) {
            const auto kind = texture->kind();
            if (!texture->prepareAttachment(context))
                throw std::runtime_error("invalid attachment");

            if (kind.depth || kind.stencil) {
                auto &depthStencil = renderPassOptions.depthStencilAttachment;
                Q_ASSERT(!depthStencil.view.isValid());
                depthStencil = {
                    .view = texture->textureView(),
                    .depthLoadOperation = KDGpu::AttachmentLoadOperation::Load,
                    .stencilLoadOperation = KDGpu::AttachmentLoadOperation::Load,
                    .initialLayout = texture->currentLayout(),
                };
            }
            else {
                renderPassOptions.colorAttachments.push_back(KDGpu::ColorAttachment{
                    .view = texture->textureView(),
                    .loadOperation = KDGpu::AttachmentLoadOperation::Load,
                    .initialLayout = texture->currentLayout(),
                });
            }
            mUsedItems += texture->usedItems();
        }

    return renderPassOptions;
}

std::vector<KDGpu::RenderTargetOptions> VKTarget::getRenderTargetOptions()
{
    auto options = std::vector<KDGpu::RenderTargetOptions>{ };
    for (const auto &attachment : mAttachments) {
        const auto kind = attachment.texture->kind();
        if (!kind.depth && !kind.stencil) {
            options.push_back({
                .format = toKDGpu(attachment.texture->format()),
                .writeMask = static_cast<KDGpu::ColorComponentFlagBits>(
                    attachment.colorWriteMask),
                .blending = {
                    .blendingEnabled = true,
                    .color = {
                        toKDGpu(attachment.blendColorEq), 
                        toKDGpu(attachment.blendColorSource),
                        toKDGpu(attachment.blendColorDest)
                    },
                    .alpha = {
                        toKDGpu(attachment.blendAlphaEq), 
                        toKDGpu(attachment.blendAlphaSource),
                        toKDGpu(attachment.blendAlphaDest)
                    }
                }
            });
        }
    }
    return options;
}

KDGpu::DepthStencilOptions VKTarget::getDepthStencilOptions()
{
    auto options = KDGpu::DepthStencilOptions{ };
    for (const auto &attachment : mAttachments) {
        const auto kind = attachment.texture->kind();
        if (kind.depth || kind.stencil) {
            if (options.format != KDGpu::Format::UNDEFINED) {
                mMessages += MessageList::insert(mItemId, MessageType::MoreThanOneDepthStencilAttachment);
                continue;
            }
            options = {
                .format = toKDGpu(attachment.texture->format()),
                .depthTestEnabled = kind.depth,
                .depthWritesEnabled = attachment.depthWrite,
                .depthCompareOperation = toKDGpu(attachment.depthComparisonFunc),
                .stencilTestEnabled = kind.stencil,
                .stencilFront = {
                    .failOp = toKDGpu(attachment.stencilFrontFailOp),
                    .passOp = toKDGpu(attachment.stencilFrontDepthPassOp),
                    .depthFailOp = toKDGpu(attachment.stencilFrontDepthFailOp),
                    .compareOp = toKDGpu(attachment.stencilFrontComparisonFunc),
                    .compareMask = attachment.stencilFrontReadMask,
                    .writeMask = attachment.stencilFrontWriteMask,
                    .reference = static_cast<uint32_t>(attachment.stencilFrontReference),
                },
                .stencilBack = {
                    .failOp = toKDGpu(attachment.stencilBackFailOp),
                    .passOp = toKDGpu(attachment.stencilBackDepthPassOp),
                    .depthFailOp = toKDGpu(attachment.stencilBackDepthFailOp),
                    .compareOp = toKDGpu(attachment.stencilBackComparisonFunc),
                    .compareMask = attachment.stencilBackReadMask,
                    .writeMask = attachment.stencilBackWriteMask,
                    .reference = static_cast<uint32_t>(attachment.stencilBackReference),
                },
                .resolveDepthStencil = false,
            };
        }
    }
    return options;
}

KDGpu::MultisampleOptions VKTarget::getMultisampleOptions()
{
    // TODO:
    return {};
}

KDGpu::PrimitiveOptions VKTarget::getPrimitiveOptions()
{
    auto depthBias = KDGpu::DepthBiasOptions{ };
    for (const auto &attachment : mAttachments) {
        if (attachment.texture->kind().depth) {
              depthBias.enabled = (attachment.depthClamp || 
                                   attachment.depthOffsetSlope || 
                                   attachment.depthOffsetConstant);
              depthBias.biasClamp = attachment.depthClamp;
              depthBias.biasSlopeFactor = attachment.depthOffsetSlope;
              depthBias.biasConstantFactor = attachment.depthOffsetConstant;
        }
    }

    return KDGpu::PrimitiveOptions{
        .cullMode = toKDGpu(mCullMode),
        .frontFace = toKDGpu(mFrontFace),
        .polygonMode = toKDGpu(mPolygonMode),
        .depthBias = depthBias,
        .lineWidth = 1.0f
    };
}