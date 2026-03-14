
#include "D3DPipeline.h"
#include "D3DBuffer.h"
#include "D3DProgram.h"
#include "D3DStream.h"
#include "D3DTarget.h"
#include "D3DTexture.h"
#include "D3DAccelerationStructure.h"
#include <QScopeGuard>

namespace {
    D3D12_FILTER getFilter(Binding::Filter min, Binding::Filter mag,
        bool anisotropic)
    {
        using Filter = Binding::Filter;
        switch (min) {
        case Filter::Nearest:
        case Filter::NearestMipMapNearest:
            return (mag == Filter::Linear
                    ? D3D12_FILTER_MIN_POINT_MAG_LINEAR_MIP_POINT
                    : anisotropic ? D3D12_FILTER_MIN_MAG_ANISOTROPIC_MIP_POINT
                                  : D3D12_FILTER_MIN_MAG_MIP_POINT);
        case Filter::NearestMipMapLinear:
            return (mag == Filter::Linear
                    ? D3D12_FILTER_MIN_POINT_MAG_MIP_LINEAR
                    : D3D12_FILTER_MIN_MAG_POINT_MIP_LINEAR);
        case Filter::LinearMipMapNearest:
            return (mag == Filter::Linear
                    ? D3D12_FILTER_MIN_MAG_LINEAR_MIP_POINT
                    : D3D12_FILTER_MIN_LINEAR_MAG_MIP_POINT);
        case Filter::Linear:
        case Filter::LinearMipMapLinear:
            return (mag == Filter::Nearest
                    ? D3D12_FILTER_MIN_LINEAR_MAG_POINT_MIP_LINEAR
                    : anisotropic ? D3D12_FILTER_ANISOTROPIC
                                  : D3D12_FILTER_MIN_MAG_MIP_LINEAR);
        }
        return D3D12_FILTER_MIN_MAG_MIP_POINT;
    }

    D3D12_STATIC_BORDER_COLOR getStaticBorderColor(const QColor &color)
    {
        if (color.alpha() == 0)
            return D3D12_STATIC_BORDER_COLOR_TRANSPARENT_BLACK;
        if (color == QColor(Qt::black))
            return D3D12_STATIC_BORDER_COLOR_OPAQUE_BLACK;
        return D3D12_STATIC_BORDER_COLOR_OPAQUE_WHITE;
    }

    D3D12_SHADER_VISIBILITY stageToVisibility(Shader::ShaderType stage)
    {
        using ST = Shader::ShaderType;
        switch (stage) {
        case ST::Vertex:         return D3D12_SHADER_VISIBILITY_VERTEX;
        case ST::Fragment:       return D3D12_SHADER_VISIBILITY_PIXEL;
        case ST::TessControl:    return D3D12_SHADER_VISIBILITY_HULL;
        case ST::TessEvaluation: return D3D12_SHADER_VISIBILITY_DOMAIN;
        case ST::Geometry:       return D3D12_SHADER_VISIBILITY_GEOMETRY;
        case ST::Task:           return D3D12_SHADER_VISIBILITY_AMPLIFICATION;
        case ST::Mesh:           return D3D12_SHADER_VISIBILITY_MESH;
        default:                 break;
        }
        return D3D12_SHADER_VISIBILITY_ALL;
    }

    D3D12_DESCRIPTOR_RANGE_TYPE getRangeType(D3D_SHADER_INPUT_TYPE inputType)
    {
        switch (inputType) {
        case D3D_SIT_CBUFFER:     return D3D12_DESCRIPTOR_RANGE_TYPE_CBV;
        case D3D_SIT_TEXTURE:
        case D3D_SIT_STRUCTURED:
        case D3D_SIT_BYTEADDRESS: return D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
        default:                  return D3D12_DESCRIPTOR_RANGE_TYPE_UAV;
        }
    }

} // namespace

D3DPipeline::D3DPipeline(ItemId itemId, D3DProgram *program)
    : PipelineBase(itemId)
    , mProgram(*program)
{
}

D3DPipeline::~D3DPipeline() = default;

bool D3DPipeline::createGraphics(D3DContext &context,
    Call::PrimitiveType primitiveType, D3DTarget *target,
    D3DStream *vertexStream)
{
    if (std::exchange(mCreated, true))
        return (mPipelineState && mRootSignature);

    mVertexStream = vertexStream;

    auto state = D3D12_GRAPHICS_PIPELINE_STATE_DESC{};
    state.RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
    state.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT);
    state.DepthStencilState = CD3DX12_DEPTH_STENCIL_DESC(D3D12_DEFAULT);
    state.DepthStencilState.DepthEnable = FALSE;

    if (!mProgram.setupPipelineState(state))
        return false;

    if (target && !target->setupPipelineState(state))
        return false;

    auto inputLayout = std::vector<D3D12_INPUT_ELEMENT_DESC>{};
    if (!createInputLayout(&inputLayout))
        return false;
    state.InputLayout = { inputLayout.data(),
        static_cast<UINT>(inputLayout.size()) };

    if (!createRootSignature(context))
        return false;

    createDescriptorHeap(context);

    state.pRootSignature = mRootSignature.Get();
    state.PrimitiveTopologyType = toD3DTopologyType(primitiveType);

    if (FAILED(context.device.CreateGraphicsPipelineState(&state,
            IID_PPV_ARGS(&mPipelineState)))) {
        mMessages +=
            MessageList::insert(mItemId, MessageType::CreatingPipelineFailed);
        return false;
    }
    return true;
}

bool D3DPipeline::createCompute(D3DContext &context)
{
    if (std::exchange(mCreated, true))
        return (mPipelineState && mRootSignature);

    auto state = D3D12_COMPUTE_PIPELINE_STATE_DESC{};

    if (!mProgram.setupPipelineState(state))
        return false;

    if (!createRootSignature(context))
        return false;

    createDescriptorHeap(context);

    state.pRootSignature = mRootSignature.Get();

    if (FAILED(context.device.CreateComputePipelineState(&state,
            IID_PPV_ARGS(&mPipelineState)))) {
        mMessages +=
            MessageList::insert(mItemId, MessageType::CreatingPipelineFailed);
        return false;
    }
    return true;
}

bool D3DPipeline::createInputLayout(
    std::vector<D3D12_INPUT_ELEMENT_DESC> *inputLayout)
{
    const auto vertexShader = mProgram.getVertexShader();
    if (!vertexShader)
        return true;

    auto reflection = vertexShader->d3dReflection();
    auto desc = D3D12_SHADER_DESC{};
    reflection->GetDesc(&desc);

    auto canRender = true;
    for (auto i = 0u; i < desc.InputParameters; ++i) {
        auto paramDesc = D3D12_SIGNATURE_PARAMETER_DESC{};
        reflection->GetInputParameterDesc(i, &paramDesc);

        if (paramDesc.SystemValueType != D3D_NAME_UNDEFINED)
            continue;

        auto semanticName = QString(paramDesc.SemanticName);
        const auto attribute = (mVertexStream
                ? mVertexStream->findAttribute(semanticName,
                      paramDesc.SemanticIndex)
                : nullptr);
        if (!attribute || !attribute->buffer) {
            if (paramDesc.SemanticIndex)
                semanticName += QString::number(paramDesc.SemanticIndex);
            mMessages += MessageList::insert(mItemId,
                MessageType::AttributeNotSet, semanticName);
            canRender = false;
            continue;
        }
        mUsedItems += attribute->usedItems;

        inputLayout->push_back(D3D12_INPUT_ELEMENT_DESC{
            .SemanticName = paramDesc.SemanticName,
            .SemanticIndex = paramDesc.SemanticIndex,
            .Format = toDXGIFormat(attribute->type, attribute->count),
            .InputSlot = i,
            .AlignedByteOffset = static_cast<UINT>(attribute->offset),
            .InputSlotClass = (attribute->divisor >= 1
                    ? D3D12_INPUT_CLASSIFICATION_PER_INSTANCE_DATA
                    : D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA),
            .InstanceDataStepRate = static_cast<UINT>(attribute->divisor),
        });
    }
    return canRender;
}

bool D3DPipeline::createRootSignature(D3DContext &context)
{
    struct DescriptorTableDesc
    {
        D3D12_SHADER_VISIBILITY visibility;
        std::vector<CD3DX12_DESCRIPTOR_RANGE> ranges;
    };
    auto descriptorTableDescs = std::vector<DescriptorTableDesc>();
    auto staticSamplers = std::vector<CD3DX12_STATIC_SAMPLER_DESC>();
    auto descriptorHeapOffset = UINT{};
    for (const auto [stage, reflection] : mProgram.d3dReflection()) {
        const auto visibility = stageToVisibility(stage);
        auto rangesPerType =
            std::vector<std::vector<CD3DX12_DESCRIPTOR_RANGE>>();
        auto shaderDesc = D3D12_SHADER_DESC{};
        reflection->GetDesc(&shaderDesc);
        for (auto i = 0u; i < shaderDesc.BoundResources; ++i) {
            auto bindDesc = D3D12_SHADER_INPUT_BIND_DESC{};
            reflection->GetResourceBindingDesc(i, &bindDesc);

            if (bindDesc.Type == D3D_SIT_SAMPLER) {
                auto &sampler = staticSamplers.emplace_back(bindDesc.BindPoint);
                sampler.ShaderRegister = bindDesc.BindPoint;
                sampler.RegisterSpace = bindDesc.Space;

                // TODO: find better solution - demangle _uTexture_sampler
                auto name = QString(bindDesc.Name);
                name = name.remove(QRegularExpression("^_"));
                name = name.remove(QRegularExpression("_sampler$"));

                if (auto binding = find(mBindings.samplers, name)) {
                    mUsedItems += binding->bindingItemId;

                    sampler.Filter = getFilter(binding->minFilter,
                        binding->magFilter, binding->anisotropic),
                    sampler.AddressU = toD3D(binding->wrapModeX);
                    sampler.AddressV = toD3D(binding->wrapModeY);
                    sampler.AddressW = toD3D(binding->wrapModeZ);
                    sampler.MipLODBias = 0;
                    sampler.MaxAnisotropy = (binding->anisotropic ? 8 : 0);
                    sampler.ComparisonFunc = toD3D(binding->comparisonFunc);
                    sampler.BorderColor =
                        getStaticBorderColor(binding->borderColor);
                    // TODO: fix mip mapping
                    sampler.MaxLOD = 0;
                } else {
                    mMessages += MessageList::insert(mItemId,
                        MessageType::SamplerNotSet, bindDesc.Name);
                }
                continue;
            }

            // TODO: fix non uniform indexing
            if (!bindDesc.BindCount) {
                mMessages += MessageList::insert(mItemId,
                    MessageType::NotImplemented, "Non-Uniform Indexing");
                return false;
            }

            const auto rangeType = getRangeType(bindDesc.Type);
            if (rangesPerType.empty()
                || rangesPerType.back().back().RangeType != rangeType)
                rangesPerType.emplace_back();
            rangesPerType.back().emplace_back().Init(rangeType,
                bindDesc.BindCount, bindDesc.BindPoint, bindDesc.Space);

            mDescriptorHeapEntries.push_back({
                .visibility = visibility,
                .rangeType = rangeType,
                .space = bindDesc.Space,
                .bindPoint = bindDesc.BindPoint,
                .offset = descriptorHeapOffset,
                .count = bindDesc.BindCount,
            });
            descriptorHeapOffset += bindDesc.BindCount;
        }

        for (auto &ranges : rangesPerType) {
            auto numDescriptors = UINT{};
            for (const auto &range : ranges)
                numDescriptors += range.NumDescriptors;
            mDescriptorTableEntries.push_back(numDescriptors);
            descriptorTableDescs.push_back({
                .visibility = visibility,
                .ranges = std::move(ranges),
            });
        }
    }

    auto rootParameters = std::vector<CD3DX12_ROOT_PARAMETER>();
    for (auto &desc : descriptorTableDescs)
        rootParameters.emplace_back().InitAsDescriptorTable(
            static_cast<UINT>(desc.ranges.size()), desc.ranges.data(),
            desc.visibility);

    const auto rootSignatureDesc = CD3DX12_ROOT_SIGNATURE_DESC(
        static_cast<UINT>(rootParameters.size()), rootParameters.data(),
        static_cast<UINT>(staticSamplers.size()), staticSamplers.data(),
        D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT);
    auto rootSignatureBlob = ComPtr<ID3DBlob>();
    AssertIfFailed(D3D12SerializeRootSignature(&rootSignatureDesc,
        D3D_ROOT_SIGNATURE_VERSION_1, &rootSignatureBlob, nullptr));

    AssertIfFailed(context.device.CreateRootSignature(0,
        rootSignatureBlob->GetBufferPointer(),
        rootSignatureBlob->GetBufferSize(), IID_PPV_ARGS(&mRootSignature)));
    return true;
}

void D3DPipeline::createDescriptorHeap(D3DContext &context)
{
    const auto numDescriptors = std::accumulate(mDescriptorTableEntries.begin(),
        mDescriptorTableEntries.end(), UINT{});
    if (!numDescriptors)
        return;

    const auto descriptorHeapDesc = D3D12_DESCRIPTOR_HEAP_DESC{
        .Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV,
        .NumDescriptors = numDescriptors,
        .Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE,
        .NodeMask = 0,
    };
    AssertIfFailed(context.device.CreateDescriptorHeap(&descriptorHeapDesc,
        IID_PPV_ARGS(&mDescriptorHeap)));
}

UINT D3DPipeline::getDescriptorHeapOffset(D3D12_SHADER_VISIBILITY visibility,
    D3D_SHADER_INPUT_TYPE inputType, UINT space, UINT bindPoint) const
{
    const auto rangeType = getRangeType(inputType);
    const auto it = std::find_if(mDescriptorHeapEntries.begin(),
        mDescriptorHeapEntries.end(), [&](const DescriptorHeapEntry &entry) {
            return (entry.visibility == visibility
                && entry.rangeType == rangeType && entry.space == space
                && entry.bindPoint == bindPoint);
        });
    Q_ASSERT(it != mDescriptorHeapEntries.end());
    return it->offset;
}

bool D3DPipeline::setDescriptors(D3DContext &context,
    ScriptEngine &scriptEngine)
{
    if (!mDescriptorHeap)
        return true;

    const auto heapStart = CD3DX12_CPU_DESCRIPTOR_HANDLE{
        mDescriptorHeap->GetCPUDescriptorHandleForHeapStart()
    };

    auto canRender = true;
    for (const auto [stage, reflection] : mProgram.d3dReflection()) {
        const auto visibility = stageToVisibility(stage);
        auto shaderDesc = D3D12_SHADER_DESC{};
        reflection->GetDesc(&shaderDesc);
        for (auto i = 0u; i < shaderDesc.BoundResources; ++i) {
            auto bindDesc = D3D12_SHADER_INPUT_BIND_DESC{};
            reflection->GetResourceBindingDesc(i, &bindDesc);

            if (bindDesc.Type == D3D_SIT_SAMPLER)
                continue;

            const auto name = QString(bindDesc.Name);
            auto spirvDescriptorBinding =
                mProgram.getSpirvDescriptorBinding(stage, name);
            const auto bindingName = (spirvDescriptorBinding
                    ? spirvDescriptorBinding->type_description->type_name
                    : name);

            auto descriptor = CD3DX12_CPU_DESCRIPTOR_HANDLE(heapStart,
                getDescriptorHeapOffset(visibility, bindDesc.Type,
                    bindDesc.Space, bindDesc.BindPoint),
                context.descriptorSize);

            switch (bindDesc.Type) {
            case D3D_SIT_CBUFFER: {
                auto buffer = std::add_pointer_t<D3DBuffer>{};
                if (auto bufferBinding = find(mBindings.buffers, bindingName)) {
                    buffer = static_cast<D3DBuffer *>(bufferBinding->buffer);
                    mUsedItems += bufferBinding->bindingItemId;
                    mUsedItems += bufferBinding->blockItemId;
                } else {
                    // TODO:
                    const auto arrayElement = 0;
                    auto &dynamic = getDynamicConstantBuffer(visibility,
                        bindDesc.Space, bindDesc.BindPoint, arrayElement);

                    if (auto cbuffer = reflection->GetConstantBufferByName(
                            bindDesc.Name)) {
                        auto cbufferDesc = D3D12_SHADER_BUFFER_DESC{};
                        cbuffer->GetDesc(&cbufferDesc);
                        if (!dynamic.buffer)
                            dynamic.buffer.emplace(cbufferDesc.Size);
                        Q_ASSERT(dynamic.buffer->size() == cbufferDesc.Size);
                        if (dynamic.buffer->size() == cbufferDesc.Size) {
                            // TODO: check if data changed before upload
                            auto &data = dynamic.buffer->writableData();
                            auto bufferData = std::span<std::byte>(
                                reinterpret_cast<std::byte *>(data.data()),
                                data.size());

                            if (spirvDescriptorBinding
                                && applyBufferMemberBindings(bufferData,
                                    spirvDescriptorBinding->block, arrayElement,
                                    scriptEngine))
                                buffer = &dynamic.buffer.value();
                        }
                    }
                }
                if (!buffer) {
                    mMessages += MessageList::insert(mItemId,
                        MessageType::BufferNotSet, bindingName);
                    canRender = false;
                    continue;
                }

                // TODO:
                for (auto i = 0u; i < bindDesc.BindCount; ++i) {
                    buffer->prepareConstantBufferView(context, descriptor);
                    descriptor.Offset(1, context.descriptorSize);
                }
                break;
            }

            case D3D_SIT_STRUCTURED:
            case D3D_SIT_BYTEADDRESS:
            case D3D_SIT_UAV_RWSTRUCTURED:
            case D3D_SIT_UAV_RWBYTEADDRESS: {
                auto buffer = std::add_pointer_t<D3DBuffer>{};
                if (bindingName == PrintfBase::bufferBindingName()) {
                    buffer = &mProgram.printf().getInitializedBuffer(context);
                } else if (
                    auto bufferBinding = find(mBindings.buffers, bindingName)) {
                    buffer = static_cast<D3DBuffer *>(bufferBinding->buffer);
                    mUsedItems += bufferBinding->bindingItemId;
                    mUsedItems += bufferBinding->blockItemId;
                }
                if (!buffer) {
                    mMessages += MessageList::insert(mItemId,
                        MessageType::BufferNotSet, bindingName);
                    canRender = false;
                    continue;
                }

                const auto structureByteStride =
                    (bindDesc.Type == D3D_SIT_STRUCTURED
                                || bindDesc.Type == D3D_SIT_UAV_RWSTRUCTURED
                            ? static_cast<int>(bindDesc.NumSamples)
                            : 0);
                const auto isUav = (bindDesc.Type == D3D_SIT_UAV_RWBYTEADDRESS
                    || bindDesc.Type == D3D_SIT_UAV_RWSTRUCTURED);

                if (buffer->size() < structureByteStride) {
                    // buffer smaller than structure size
                    // TODO: warn when buffer block stride is smaller
                    mMessages += MessageList::insert(mItemId,
                        MessageType::BufferNotSet, bindingName);
                    canRender = false;
                    continue;
                }

                // TODO:
                for (auto i = 0u; i < bindDesc.BindCount; ++i) {
                    if (isUav) {
                        const auto isReadonly =
                            (bindDesc.Type == D3D_SIT_STRUCTURED);
                        buffer->prepareUnorderedAccessView(context, descriptor,
                            structureByteStride, isReadonly);
                    } else {
                        buffer->prepareShaderResourceView(context, descriptor,
                            structureByteStride);
                    }
                    descriptor.Offset(1, context.descriptorSize);
                }
                break;
            }

            case D3D_SIT_TEXTURE: {
                const auto samplerBinding = find(mBindings.samplers, name);
                if (!samplerBinding || !samplerBinding->texture) {
                    mMessages += MessageList::insert(mItemId,
                        MessageType::SamplerNotSet, name);
                    canRender = false;
                    continue;
                }

                mUsedItems += samplerBinding->bindingItemId;
                mUsedItems += samplerBinding->texture->itemId();
                auto texture =
                    static_cast<D3DTexture *>(samplerBinding->texture);

                // TODO:
                for (auto i = 0u; i < bindDesc.BindCount; ++i) {
                    texture->prepareShaderResourceView(context, descriptor);
                    descriptor.Offset(1, context.descriptorSize);
                }
                break;
            }

            case D3D_SIT_UAV_RWTYPED: {
                const auto imageBinding = find(mBindings.images, name);
                if (!imageBinding || !imageBinding->texture) {
                    mMessages += MessageList::insert(mItemId,
                        MessageType::ImageNotSet, name);
                    canRender = false;
                    continue;
                }
                mUsedItems += imageBinding->bindingItemId;
                mUsedItems += imageBinding->texture->itemId();
                auto texture = static_cast<D3DTexture *>(imageBinding->texture);

                // TODO:
                for (auto i = 0u; i < bindDesc.BindCount; ++i) {
                    texture->prepareUnorderedAccessView(context, descriptor);
                    descriptor.Offset(1, context.descriptorSize);
                }
                break;
            }

            default: Q_ASSERT(!"binding type not handled"); break;
            }
        }
    }
    return canRender;
}

bool D3DPipeline::bindGraphics(D3DContext &context, ScriptEngine &scriptEngine)
{
    context.graphicsCommandList->SetPipelineState(mPipelineState.Get());
    context.graphicsCommandList->SetGraphicsRootSignature(mRootSignature.Get());

    if (!setDescriptors(context, scriptEngine))
        return false;

    if (mDescriptorHeap) {
        auto descriptorHeaps = mDescriptorHeap.Get();
        context.graphicsCommandList->SetDescriptorHeaps(1, &descriptorHeaps);

        auto descriptor = CD3DX12_GPU_DESCRIPTOR_HANDLE{
            mDescriptorHeap->GetGPUDescriptorHandleForHeapStart()
        };
        for (auto i = 0u; i < mDescriptorTableEntries.size(); ++i) {
            context.graphicsCommandList->SetGraphicsRootDescriptorTable(i,
                descriptor);
            descriptor.Offset(mDescriptorTableEntries[i],
                context.descriptorSize);
        }
    }
    return true;
}

bool D3DPipeline::bindCompute(D3DContext &context, ScriptEngine &scriptEngine)
{
    context.graphicsCommandList->SetPipelineState(mPipelineState.Get());
    context.graphicsCommandList->SetComputeRootSignature(mRootSignature.Get());

    if (!setDescriptors(context, scriptEngine))
        return false;

    if (mDescriptorHeap) {
        auto descriptorHeaps = mDescriptorHeap.Get();
        context.graphicsCommandList->SetDescriptorHeaps(1, &descriptorHeaps);

        auto descriptor = CD3DX12_GPU_DESCRIPTOR_HANDLE{
            mDescriptorHeap->GetGPUDescriptorHandleForHeapStart()
        };
        for (auto i = 0u; i < mDescriptorTableEntries.size(); ++i) {
            context.graphicsCommandList->SetComputeRootDescriptorTable(i,
                descriptor);
            descriptor.Offset(mDescriptorTableEntries[i],
                context.descriptorSize);
        }
    }
    return true;
}

auto D3DPipeline::getDynamicConstantBuffer(D3D12_SHADER_VISIBILITY visibility,
    UINT space, UINT bindPoint, UINT arrayElement) -> DynamicConstantBuffer &
{
    auto it = std::find_if(mDynamicConstantBuffers.begin(),
        mDynamicConstantBuffers.end(), [&](const auto &block) {
            return (block->visibility == visibility && block->space == space
                && block->bindPoint == bindPoint
                && block->arrayElement == arrayElement);
        });
    if (it == mDynamicConstantBuffers.end()) {
        it = mDynamicConstantBuffers.emplace(mDynamicConstantBuffers.end(),
            new DynamicConstantBuffer{
                .visibility = visibility,
                .space = space,
                .bindPoint = bindPoint,
                .arrayElement = arrayElement,
            });
    }
    return **it;
}
