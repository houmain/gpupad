#include "VKTarget.h"

namespace {
    bool isBlendingEnabled(const Attachment &attachment)
    {
        using BE = Attachment::BlendEquation;
        using BF = Attachment::BlendFactor;
        return attachment.blendColorEq != BE::Add
            || attachment.blendColorSource != BF::One
            || attachment.blendColorDest != BF::Zero
            || attachment.blendAlphaEq != BE::Add
            || attachment.blendAlphaSource != BF::One
            || attachment.blendAlphaDest != BF::Zero;
    }
} // namespace

VKTarget::VKTarget(const Target &target, VKRenderSession &renderSession)
    : mDevice(renderSession.device())
    , mItemId(target.id)
    , mFrontFace(target.frontFace)
    , mCullMode(target.cullMode)
    , mPolygonMode(target.polygonMode)
    , mLogicOperation(target.logicOperation)
    , mBlendConstant(target.blendConstant)
    , mSamples(target.defaultSamples)
{
    mUsedItems += target.id;
    mUsedItems += renderSession.session().id;

    if (renderSession.session().reverseCulling)
        mFrontFace = (mFrontFace == Target::FrontFace::CW
                ? Target::FrontFace::CCW
                : Target::FrontFace::CW);

    renderSession.evaluateTargetProperties(target, &mDefaultWidth,
        &mDefaultHeight, &mDefaultLayers);

    auto attachmentIndex = 0;
    for (const auto &item : target.items) {
        if (auto attachment = castItem<Attachment>(item)) {
            static_cast<Attachment &>(mAttachments[attachmentIndex]) =
                *attachment;
            mUsedItems += attachment->id;
        }
        attachmentIndex++;
    }
}

void VKTarget::setTexture(int index, VKTexture *texture)
{
    if (!texture)
        return;

    if (const auto first = mAttachments.first().texture;
        first && texture->samples() != first->samples()) {
        mMessages.insert(mItemId, MessageType::SampleCountMismatch);
        return;
    }

    mUsedItems += texture->usedItems();

    const auto &attachment = mAttachments[index];
    if (!texture->supportsAttachment(mDevice, isBlendingEnabled(attachment))) {
        mMessages.insert(attachment.id, MessageType::CreatingTargetFailed,
            QStringLiteral("unsupported texture format or target"));
        mInvalidAttachments = true;
        return;
    }

    const auto kind = texture->kind();
    texture->addUsage(kind.depth || kind.stencil
            ? KDGpu::TextureUsageFlagBits::DepthStencilAttachmentBit
            : KDGpu::TextureUsageFlagBits::ColorAttachmentBit);

    mAttachments[index].texture = texture;
    mSamples = texture->samples();
}

bool VKTarget::validateAttachments(VKContext &context)
{
    if (mInvalidAttachments)
        return false;

    auto colorAttachmentCount = 0u;
    auto hasDepthStencil = false;
    auto samples = 0;

    for (const auto &attachment : std::as_const(mAttachments)) {
        const auto texture = attachment.texture;
        if (!texture)
            continue;

        if (!texture->supportsAttachment(context.device,
                isBlendingEnabled(attachment))) {
            mMessages.insert(attachment.id, MessageType::CreatingTargetFailed,
                QStringLiteral("unsupported texture format or target"));
            return false;
        }

        if (attachment.level < 0 || attachment.layer < -1
            || attachment.layer >= texture->layers()) {
            mMessages.insert(attachment.id, MessageType::CreatingTargetFailed,
                QStringLiteral("invalid mipmap level or layer"));
            return false;
        }

        if (samples && samples != texture->samples()) {
            mMessages.insert(mItemId, MessageType::SampleCountMismatch);
            return false;
        }
        samples = texture->samples();

        const auto kind = texture->kind();
        if (kind.color) {
            if (++colorAttachmentCount
                > context.adapterLimits().maxColorAttachments) {
                mMessages.insert(mItemId, MessageType::TooManyColorAttachments);
                return false;
            }
        }
        if ((kind.depth || kind.stencil)
            && std::exchange(hasDepthStencil, true)) {
            mMessages.insert(mItemId,
                MessageType::MoreThanOneDepthStencilAttachment);
            return false;
        }

        const auto supportedSamples = (kind.color
                ? context.adapterLimits().framebufferColorSampleCounts
                : kind.depth
                ? context.adapterLimits().framebufferDepthSampleCounts
                : context.adapterLimits().framebufferStencilSampleCounts);
        if (!supportedSamples.testFlag(getKDSampleCount(texture->samples()))) {
            mMessages.insert(mItemId, MessageType::MaxSampleCountExceeded,
                QString::number(getKDSamples(supportedSamples)));
            return false;
        }
    }

    if (!samples
        && !context.adapterLimits()
            .framebufferNoAttachmentsSampleCounts.testFlag(
                getKDSampleCount(mSamples))) {
        const auto maxSamples = getKDSamples(
            context.adapterLimits().framebufferNoAttachmentsSampleCounts);
        mMessages.insert(mItemId, MessageType::MaxSampleCountExceeded,
            QString::number(maxSamples));
        return false;
    }
    return true;
}

bool VKTarget::prepare(VKContext &context,
    KDGpu::RenderPassCommandRecorderOptions *options)
{
    *options = KDGpu::RenderPassCommandRecorderOptions{ };

    for (auto &attachment : mAttachments)
        if (auto texture = attachment.texture) {
            const auto kind = texture->kind();
            if (!texture->prepareAttachment(context)) {
                mMessages.insert(attachment.id,
                    MessageType::CreatingTargetFailed,
                    QStringLiteral("texture creation failed"));
                return false;
            }
            if (attachment.level >= texture->levels()) {
                mMessages.insert(attachment.id,
                    MessageType::CreatingTargetFailed,
                    QStringLiteral("invalid mipmap level"));
                return false;
            }
            auto &view = texture->getView(attachment.level, attachment.layer);
            if (!view.isValid()) {
                mMessages.insert(attachment.id,
                    MessageType::CreatingTargetFailed,
                    QStringLiteral("texture view creation failed"));
                return false;
            }
            if (kind.depth || kind.stencil) {
                auto &depthStencil = options->depthStencilAttachment;

                if (depthStencil.view.isValid()) {
                    mMessages.insert(mItemId,
                        MessageType::MoreThanOneDepthStencilAttachment);
                    continue;
                }

                depthStencil = {
                    .view = view,
                    .depthLoadOperation = KDGpu::AttachmentLoadOperation::Load,
                    .stencilLoadOperation =
                        KDGpu::AttachmentLoadOperation::Load,
                    .initialLayout = texture->currentLayout(),
                };
            } else {
                options->colorAttachments.push_back(KDGpu::ColorAttachment{
                    .view = view,
                    .loadOperation = KDGpu::AttachmentLoadOperation::Load,
                    .initialLayout = texture->currentLayout(),
                });
            }

            const auto min = [](uint32_t &var, const uint32_t value) {
                var = (var == 0 || value < var ? value : var);
            };
            min(options->framebufferWidth,
                static_cast<uint32_t>(
                    std::max(texture->width() >> attachment.level, 1)));
            min(options->framebufferHeight,
                static_cast<uint32_t>(
                    std::max(texture->height() >> attachment.level, 1)));
            min(options->framebufferArrayLayers,
                static_cast<uint32_t>(
                    attachment.layer >= 0 ? 1 : texture->layers()));
        }

    if (!options->framebufferWidth || !options->framebufferHeight) {
        options->framebufferWidth = mDefaultWidth;
        options->framebufferHeight = mDefaultHeight;
        options->framebufferArrayLayers = mDefaultLayers;
    }

    options->samples = getKDSampleCount(mSamples);
    return true;
}

std::vector<KDGpu::RenderTargetOptions> VKTarget::getRenderTargetOptions()
{
    auto options = std::vector<KDGpu::RenderTargetOptions>{ };
    for (const auto &attachment : std::as_const(mAttachments)) {
        if (!attachment.texture)
            continue;
        const auto kind = attachment.texture->kind();
        if (!kind.depth && !kind.stencil) {
            options.push_back({ 
                .format = toKDGpu(attachment.texture->format()),
                .writeMask = static_cast<KDGpu::ColorComponentFlagBits>(attachment.colorWriteMask),
                .blending = {
                    .blendingEnabled = isBlendingEnabled(attachment),
                    .color = { toKDGpu(attachment.blendColorEq),
                        toKDGpu(attachment.blendColorSource), toKDGpu(attachment.blendColorDest) },
                    .alpha = { toKDGpu(attachment.blendAlphaEq),
                        toKDGpu(attachment.blendAlphaSource), toKDGpu(attachment.blendAlphaDest) },
                }, 
            });
        }
    }
    return options;
}

KDGpu::DepthStencilOptions VKTarget::getDepthStencilOptions()
{
    auto options = KDGpu::DepthStencilOptions{ };
    for (const auto &attachment : std::as_const(mAttachments)) {
        if (!attachment.texture)
            continue;
        const auto kind = attachment.texture->kind();
        if (kind.depth || kind.stencil) {
            if (options.format != KDGpu::Format::UNDEFINED) {
                mMessages.insert(mItemId,
                    MessageType::MoreThanOneDepthStencilAttachment);
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
    return {
        .samples = getKDSampleCount(mSamples),
    };
}

KDGpu::PrimitiveOptions VKTarget::getPrimitiveOptions()
{
    auto depthBias = KDGpu::DepthBiasOptions{ };
    for (const auto &attachment : std::as_const(mAttachments))
        if (attachment.texture && attachment.texture->kind().depth) {
            depthBias.enabled = (attachment.depthClamp
                || attachment.depthOffsetSlope
                || attachment.depthOffsetConstant);
            depthBias.biasClamp = attachment.depthClamp;
            depthBias.biasSlopeFactor = attachment.depthOffsetSlope;
            depthBias.biasConstantFactor = attachment.depthOffsetConstant;
        }

    return KDGpu::PrimitiveOptions{
        .cullMode = toKDGpu(mCullMode),
        .frontFace = toKDGpu(mFrontFace),
        .polygonMode = toKDGpu(mPolygonMode),
        .depthBias = depthBias,
        .lineWidth = 1.0f,
    };
}

bool VKTarget::hasAttachment(const VKTexture *texture) const
{
    for (const auto &attachment : mAttachments)
        if (attachment.texture == texture)
            return true;
    return false;
}
