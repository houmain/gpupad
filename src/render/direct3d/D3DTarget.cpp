#include "D3DTarget.h"

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

    D3D12_STENCIL_OP toD3DStencilOp(Attachment::StencilOperation stencilOp)
    {
        using SO = Attachment::StencilOperation;
        switch (stencilOp) {
        case SO::Keep:          return D3D12_STENCIL_OP_KEEP;
        case SO::Zero:          return D3D12_STENCIL_OP_ZERO;
        case SO::Replace:       return D3D12_STENCIL_OP_REPLACE;
        case SO::Increment:     return D3D12_STENCIL_OP_INCR_SAT;
        case SO::IncrementWrap: return D3D12_STENCIL_OP_INCR;
        case SO::Decrement:     return D3D12_STENCIL_OP_DECR_SAT;
        case SO::DecrementWrap: return D3D12_STENCIL_OP_DECR;
        case SO::Invert:        return D3D12_STENCIL_OP_INVERT;
        }
        Q_ASSERT(!"not handled");
        return D3D12_STENCIL_OP_KEEP;
    }

    D3D12_DEPTH_STENCILOP_DESC toD3DDepthStencilOpDesc(
        Attachment::StencilOperation stencilFrontFailOp,
        Attachment::StencilOperation stencilFrontDepthFailOp,
        Attachment::StencilOperation stencilFrontDepthPassOp,
        Attachment::ComparisonFunc stencilFrontComparisonFunc)
    {
        return {
            .StencilFailOp = toD3DStencilOp(stencilFrontFailOp),
            .StencilDepthFailOp = toD3DStencilOp(stencilFrontDepthFailOp),
            .StencilPassOp = toD3DStencilOp(stencilFrontDepthPassOp),
            .StencilFunc = toD3D(stencilFrontComparisonFunc),
        };
    }
} // namespace

D3DTarget::D3DTarget(const Target &target, D3DRenderSession &renderSession)
    : mItemId(target.id)
    , mFrontFace(target.frontFace)
    , mCullMode(target.cullMode)
    , mPolygonMode(target.polygonMode)
    , mLogicOperation(target.logicOperation)
    , mBlendConstant(target.blendConstant)
    , mSamples(target.defaultSamples)
{
    mUsedItems += target.id;
    mUsedItems += renderSession.session().id;

    mFlipViewport = renderSession.session().flipViewport;

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

void D3DTarget::setTexture(int index, D3DTexture *texture)
{
    if (!texture)
        return;

    if (const auto first = mAttachments.first().texture;
        first && texture->samples() != first->samples()) {
        mMessages.insert(mItemId, MessageType::SampleCountMismatch);
        return;
    }

    mAttachments[index].texture = texture;
    mSamples = texture->samples();
    mUsedItems += texture->usedItems();
}

bool D3DTarget::setupPipelineState(D3DContext &context,
    D3D12_GRAPHICS_PIPELINE_STATE_DESC &state)
{
    state.RasterizerState.FillMode = toD3D(mPolygonMode);
    state.RasterizerState.CullMode = toD3D(mCullMode);
    state.RasterizerState.FrontCounterClockwise =
        isFrontCouterClockwise(mFrontFace);
    state.SampleMask = UINT_MAX;
    state.SampleDesc = toDXGISampleDesc(mSamples);
    state.NumRenderTargets = 0;
    auto samples = 0;
    auto hasDepthStencil = false;
    for (auto &attachment : mAttachments)
        if (auto texture = attachment.texture) {
            const auto kind = texture->kind();
            const auto format = toDXGIFormat(texture->format());
            const auto blending = isBlendingEnabled(attachment);
            const auto viewDimension = (kind.depth || kind.stencil
                    ? texture->depthStencilViewDesc().ViewDimension
                    : texture->renderTargetViewDesc().ViewDimension);

            if (!format || !viewDimension) {
                mMessages.insert(attachment.id,
                    MessageType::CreatingTargetFailed,
                    QStringLiteral("unsupported texture format or target"));
                return false;
            }

            if (samples && samples != texture->samples()) {
                mMessages.insert(mItemId, MessageType::SampleCountMismatch);
                return false;
            }
            samples = texture->samples();

            auto formatSupport = D3D12_FEATURE_DATA_FORMAT_SUPPORT{
                .Format = format,
            };
            if (FAILED(context.device.CheckFeatureSupport(
                    D3D12_FEATURE_FORMAT_SUPPORT, &formatSupport,
                    sizeof(formatSupport)))) {
                mMessages.insert(attachment.id,
                    MessageType::CreatingTargetFailed,
                    QStringLiteral("format capabilities unavailable"));
                return false;
            }

            auto requiredSupport = D3D12_FORMAT_SUPPORT1_NONE;
            switch (texture->target()) {
            case Texture::Target::Target1D:
            case Texture::Target::Target1DArray:
                requiredSupport |= D3D12_FORMAT_SUPPORT1_TEXTURE1D;
                break;
            case Texture::Target::Target3D:
                requiredSupport |= D3D12_FORMAT_SUPPORT1_TEXTURE3D;
                break;
            default:
                requiredSupport |= D3D12_FORMAT_SUPPORT1_TEXTURE2D;
                break;
            }
            requiredSupport |= (kind.depth || kind.stencil
                    ? D3D12_FORMAT_SUPPORT1_DEPTH_STENCIL
                    : D3D12_FORMAT_SUPPORT1_RENDER_TARGET);
            if (blending)
                requiredSupport |= D3D12_FORMAT_SUPPORT1_BLENDABLE;
            if (texture->samples() > 1)
                requiredSupport |=
                    D3D12_FORMAT_SUPPORT1_MULTISAMPLE_RENDERTARGET;

            if ((formatSupport.Support1 & requiredSupport)
                != requiredSupport) {
                mMessages.insert(attachment.id,
                    MessageType::CreatingTargetFailed,
                    QStringLiteral("unsupported texture format or usage"));
                return false;
            }

            if (texture->samples() > 1) {
                auto sampleSupport =
                    D3D12_FEATURE_DATA_MULTISAMPLE_QUALITY_LEVELS{
                        .Format = format,
                        .SampleCount =
                            static_cast<UINT>(texture->samples()),
                    };
                if (FAILED(context.device.CheckFeatureSupport(
                        D3D12_FEATURE_MULTISAMPLE_QUALITY_LEVELS,
                        &sampleSupport, sizeof(sampleSupport)))
                    || !sampleSupport.NumQualityLevels) {
                    mMessages.insert(mItemId,
                        MessageType::MaxSampleCountExceeded,
                        QString::number(texture->samples()));
                    return false;
                }
            }

            if (kind.depth || kind.stencil) {
                if (std::exchange(hasDepthStencil, true)) {
                    mMessages.insert(mItemId,
                        MessageType::MoreThanOneDepthStencilAttachment);
                    return false;
                }
                state.DSVFormat = format;
                state.DepthStencilState = {
                    .DepthEnable = (kind.depth),
                    .DepthWriteMask = (attachment.depthWrite
                            ? D3D12_DEPTH_WRITE_MASK_ALL
                            : D3D12_DEPTH_WRITE_MASK_ZERO),
                    .DepthFunc = toD3D(attachment.depthComparisonFunc),
                    .StencilEnable = (kind.stencil),
                    .StencilReadMask =
                        static_cast<UINT8>(attachment.stencilFrontReadMask),
                    .StencilWriteMask =
                        static_cast<UINT8>(attachment.stencilFrontWriteMask),
                    .FrontFace =
                        toD3DDepthStencilOpDesc(attachment.stencilFrontFailOp,
                            attachment.stencilFrontDepthFailOp,
                            attachment.stencilFrontDepthPassOp,
                            attachment.stencilFrontComparisonFunc),
                    .BackFace =
                        toD3DDepthStencilOpDesc(attachment.stencilBackFailOp,
                            attachment.stencilBackDepthFailOp,
                            attachment.stencilBackDepthPassOp,
                            attachment.stencilBackComparisonFunc),
                };
                // TODO:
                // stencilFrontReference, stencilBackReference, stencilBackReadMask, stencilBackWriteMask
            } else if (kind.color) {
                if (state.NumRenderTargets == 8) {
                    mMessages.insert(mItemId,
                        MessageType::TooManyColorAttachments);
                    return false;
                }

                const auto target = state.NumRenderTargets++;
                state.BlendState.RenderTarget[target] = {
                    .BlendEnable = blending,
                    //.LogicOpEnable = (mLogicOperation
                    //    != Target::LogicOperation::NoLogicOperation),
                    .SrcBlend = toD3D(attachment.blendColorSource),
                    .DestBlend = toD3D(attachment.blendColorDest),
                    .BlendOp = toD3D(attachment.blendColorEq),
                    .SrcBlendAlpha = toD3D(attachment.blendAlphaSource),
                    .DestBlendAlpha = toD3D(attachment.blendAlphaDest),
                    .BlendOpAlpha = toD3D(attachment.blendAlphaEq),
                    //.LogicOp = toD3D(mLogicOperation),
                    .RenderTargetWriteMask =
                        static_cast<UINT8>(attachment.colorWriteMask),
                };

                state.RTVFormats[target] = format;
            }
        }
    return true;
}

bool D3DTarget::bind(D3DContext &context)
{
    auto renderTargets = std::vector<ID3D12Resource *>();
    auto renderTargetViewDescs = std::vector<D3D12_RENDER_TARGET_VIEW_DESC>();
    auto depthStencilView = std::add_pointer_t<ID3D12Resource>{};
    auto depthStencilViewDesc = D3D12_DEPTH_STENCIL_VIEW_DESC{};
    auto width = 0;
    auto height = 0;

    for (auto &attachment : mAttachments)
        if (auto texture = attachment.texture) {
            const auto kind = texture->kind();
            if (kind.depth || kind.stencil) {
                depthStencilViewDesc = texture->depthStencilViewDesc();
                if (!depthStencilViewDesc.Format
                    || !depthStencilViewDesc.ViewDimension)
                    return false;
                texture->prepareDepthStencilView(context);
                if (!texture->resource())
                    return false;

                depthStencilView = texture->resource();
            } else if (kind.color) {
                const auto renderTargetViewDesc =
                    texture->renderTargetViewDesc();
                if (!renderTargetViewDesc.Format
                    || !renderTargetViewDesc.ViewDimension)
                    return false;
                texture->prepareRenderTargetView(context);
                if (!texture->resource())
                    return false;

                renderTargetViewDescs.push_back(renderTargetViewDesc);
                renderTargets.push_back(texture->resource());
            }

            const auto min = [](int &var, const int value) {
                var = (var == 0 || value < var ? value : var);
            };
            min(width, texture->width());
            min(height, texture->height());
        }

    if (!width)
        width = mDefaultWidth;
    if (!height)
        height = mDefaultHeight;

    context.renderTargetHelper.OMSetRenderTargets(
        context.graphicsCommandList.Get(),
        static_cast<DWORD>(renderTargets.size()), renderTargets.data(),
        renderTargetViewDescs.data(), depthStencilView,
        depthStencilView ? &depthStencilViewDesc : nullptr);

    auto viewport = D3D12_VIEWPORT{
        .TopLeftX = 0,
        .TopLeftY = 0,
        .Width = static_cast<float>(width),
        .Height = static_cast<float>(height),
        .MinDepth = 0.0f,
        .MaxDepth = 1.0f,
    };

    if (mFlipViewport) {
        viewport.TopLeftY = viewport.Height;
        viewport.Height = -viewport.Height;
    }

    context.graphicsCommandList->RSSetViewports(1, &viewport);

    const auto scissorRect = D3D12_RECT{ 0, 0, width, height };
    context.graphicsCommandList->RSSetScissorRects(1, &scissorRect);
    return true;
}

bool D3DTarget::hasAttachment(const D3DTexture *texture) const
{
    for (const auto &attachment : mAttachments)
        if (attachment.texture == texture)
            return true;
    return false;
}
