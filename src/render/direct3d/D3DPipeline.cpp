
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

    bool isGlobalConstantsBufferName(const QString &name)
    {
        return (name == "$Globals" || name == "_Global");
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

    createGlobalConstantBuffers(context);
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

    createGlobalConstantBuffers(context);
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

    auto reflection = vertexShader->reflection();
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
    for (const auto [stage, reflection] : mProgram.reflection()) {
        const auto visibility = stageToVisibility(stage);
        auto ranges = std::vector<CD3DX12_DESCRIPTOR_RANGE>();
        auto shaderDesc = D3D12_SHADER_DESC{};
        reflection->GetDesc(&shaderDesc);
        for (auto i = 0u; i < shaderDesc.BoundResources; ++i) {
            auto bindDesc = D3D12_SHADER_INPUT_BIND_DESC{};
            reflection->GetResourceBindingDesc(i, &bindDesc);

            if (bindDesc.Type == D3D_SIT_SAMPLER) {
                auto &sampler = staticSamplers.emplace_back(bindDesc.BindPoint);
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

            const auto rangeType = [&]() {
                switch (bindDesc.Type) {
                case D3D_SIT_CBUFFER: return D3D12_DESCRIPTOR_RANGE_TYPE_CBV;
                case D3D_SIT_TEXTURE: return D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
                default:              return D3D12_DESCRIPTOR_RANGE_TYPE_UAV;
                }
            }();

            // TODO: fix non uniform indexing
            if (!bindDesc.BindCount) {
                mMessages += MessageList::insert(mItemId,
                    MessageType::NotImplemented, "Non-Uniform Indexing");
                return false;
            }

            ranges.emplace_back().Init(rangeType, bindDesc.BindCount,
                bindDesc.BindPoint, bindDesc.Space);

            mDescriptorHeapEntries.push_back({
                .visibility = visibility,
                .space = bindDesc.Space,
                .bindPoint = bindDesc.BindPoint,
                .offset = descriptorHeapOffset,
            });
            descriptorHeapOffset += bindDesc.BindCount;
        }

        const auto numDescriptors = descriptorHeapOffset
            - (!mDescriptorTableEntries.empty() ? mDescriptorTableEntries.back()
                                                : 0);
        if (numDescriptors) {
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

void D3DPipeline::createGlobalConstantBuffers(D3DContext &context)
{
    for (const auto [stage, reflection] : mProgram.reflection()) {
        auto shaderDesc = D3D12_SHADER_DESC{};
        reflection->GetDesc(&shaderDesc);
        for (auto i = 0u; i < shaderDesc.ConstantBuffers; ++i) {
            const auto cbuffer = reflection->GetConstantBufferByIndex(i);
            auto cbufferDesc = D3D12_SHADER_BUFFER_DESC{};
            cbuffer->GetDesc(&cbufferDesc);
            if (cbufferDesc.Type != D3D_SIT_CBUFFER
                || !isGlobalConstantsBufferName(cbufferDesc.Name))
                continue;

            auto &buffer = mGlobalConstantBuffers[stage];
            auto size = UINT{ 1 };
            for (auto v = 0u; v < cbufferDesc.Variables; ++v) {
                const auto var = cbuffer->GetVariableByIndex(v);
                auto varDesc = D3D12_SHADER_VARIABLE_DESC{};
                var->GetDesc(&varDesc);
                size = std::max(size, varDesc.StartOffset + varDesc.Size);
            }
            buffer = std::make_unique<D3DBuffer>(size);
            buffer->initialize(context);
        }
    }
}

void writeValue(QByteArray &data, const D3D12_SHADER_VARIABLE_DESC &varDesc,
    const D3D12_SHADER_TYPE_DESC &typeDesc, ScriptEngine &scriptEngine,
    const UniformBinding &binding)
{
    Q_ASSERT(varDesc.StartOffset + varDesc.Size <= data.size());
    const auto count = typeDesc.Rows * typeDesc.Columns;
    if (typeDesc.Type == D3D_SVT_FLOAT) {
        const auto values = getValues<float>(scriptEngine, binding.values, 0,
            count, binding.bindingItemId);
        std::memcpy(data.data() + varDesc.StartOffset, values.data(),
            varDesc.Size);
    } else if (typeDesc.Type == D3D_SVT_INT) {
        const auto values = getValues<int>(scriptEngine, binding.values, 0,
            count, binding.bindingItemId);
        std::memcpy(data.data() + varDesc.StartOffset, values.data(),
            varDesc.Size);
    } else {
        Q_ASSERT("unhandled datatype");
    }
}

void D3DPipeline::updateGlobalConstantBuffers(D3DContext &context,
    ScriptEngine &scriptEngine)
{
    for (const auto [stage, reflection] : mProgram.reflection()) {
        auto shaderDesc = D3D12_SHADER_DESC{};
        reflection->GetDesc(&shaderDesc);
        for (auto i = 0u; i < shaderDesc.ConstantBuffers; ++i) {
            const auto cbuffer = reflection->GetConstantBufferByIndex(i);
            auto cbufferDesc = D3D12_SHADER_BUFFER_DESC{};
            cbuffer->GetDesc(&cbufferDesc);
            if (cbufferDesc.Type != D3D_SIT_CBUFFER
                || !isGlobalConstantsBufferName(cbufferDesc.Name))
                continue;

            auto buffer = getGlobalConstantBuffer(stage);
            auto &data = buffer->writableData();
            for (auto v = 0u; v < cbufferDesc.Variables; ++v) {
                const auto var = cbuffer->GetVariableByIndex(v);
                auto varDesc = D3D12_SHADER_VARIABLE_DESC{};
                var->GetDesc(&varDesc);

                const auto type = var->GetType();
                auto typeDesc = D3D12_SHADER_TYPE_DESC{};
                type->GetDesc(&typeDesc);

                auto name = QString(varDesc.Name);

                // TODO: find better solution - demangle _44_uPerspective
                name = name.remove(QRegularExpression("^_\\d+_"));

                if (auto uniformBinding = find(mBindings.uniforms, name)) {
                    mUsedItems += uniformBinding->bindingItemId;
                    writeValue(data, varDesc, typeDesc, scriptEngine,
                        *uniformBinding);
                } else {
                    mMessages += MessageList::insert(mItemId,
                        MessageType::UniformNotSet, name);
                }
            }
        }
    }
}

D3DBuffer *D3DPipeline::getGlobalConstantBuffer(Shader::ShaderType stage)
{
    return mGlobalConstantBuffers[stage].get();
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
    UINT space, UINT bindPoint) const
{
    const auto it = std::find_if(mDescriptorHeapEntries.begin(),
        mDescriptorHeapEntries.end(), [&](const DescriptorHeapEntry &entry) {
            return (entry.visibility == visibility && entry.space == space
                && entry.bindPoint == bindPoint);
        });
    Q_ASSERT(it != mDescriptorHeapEntries.end());
    return it->offset;
}

bool D3DPipeline::setDescriptors(D3DContext &context)
{
    if (!mDescriptorHeap)
        return true;

    const auto heapStart = CD3DX12_CPU_DESCRIPTOR_HANDLE{
        mDescriptorHeap->GetCPUDescriptorHandleForHeapStart()
    };

    auto canRender = true;
    for (const auto [stage, reflection] : mProgram.reflection()) {
        const auto visibility = stageToVisibility(stage);
        auto shaderDesc = D3D12_SHADER_DESC{};
        reflection->GetDesc(&shaderDesc);
        for (auto i = 0u; i < shaderDesc.BoundResources; ++i) {
            auto bindDesc = D3D12_SHADER_INPUT_BIND_DESC{};
            reflection->GetResourceBindingDesc(i, &bindDesc);
            const auto name = QString(bindDesc.Name);

            auto descriptor = CD3DX12_CPU_DESCRIPTOR_HANDLE(heapStart,
                getDescriptorHeapOffset(visibility, bindDesc.Space,
                    bindDesc.BindPoint),
                context.descriptorSize);

            if (bindDesc.Type == D3D_SIT_CBUFFER
                || bindDesc.Type == D3D_SIT_BYTEADDRESS
                || bindDesc.Type == D3D_SIT_UAV_RWBYTEADDRESS) {

                const auto bindingName =
                    mProgram.getBufferBindingName(stage, name);

                auto buffer = std::add_pointer_t<D3DBuffer>{};
                if (isGlobalConstantsBufferName(bindingName)) {
                    buffer = getGlobalConstantBuffer(stage);
                } else if (bindingName == ShaderPrintf::bufferBindingName()) {
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

                // TODO:
                for (auto i = 0u; i < bindDesc.BindCount; ++i) {
                    if (bindDesc.Type == D3D_SIT_CBUFFER) {
                        buffer->prepareConstantBufferView(context, descriptor);
                    } else {
                        buffer->prepareUnorderedAccessView(context, descriptor);
                    }
                    descriptor.Offset(1, context.descriptorSize);
                }

            } else if (bindDesc.Type == D3D_SIT_TEXTURE) {
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
            } else if (bindDesc.Type == D3D_SIT_UAV_RWTYPED) {
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
            } else if (bindDesc.Type == D3D_SIT_SAMPLER) {
                // nothing to do
            } else {
                Q_ASSERT(!"binding type not handled");
            }
        }
    }
    return canRender;
}

bool D3DPipeline::bindGraphics(D3DContext &context, ScriptEngine &scriptEngine)
{
    context.graphicsCommandList->SetPipelineState(mPipelineState.Get());
    context.graphicsCommandList->SetGraphicsRootSignature(mRootSignature.Get());

    updateGlobalConstantBuffers(context, scriptEngine);

    if (!setDescriptors(context))
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

    updateGlobalConstantBuffers(context, scriptEngine);

    if (!setDescriptors(context))
        return false;

    if (mDescriptorHeap) {
        auto descriptorHeaps = mDescriptorHeap.Get();
        context.graphicsCommandList->SetDescriptorHeaps(1, &descriptorHeaps);
        context.graphicsCommandList->SetComputeRootDescriptorTable(0,
            mDescriptorHeap->GetGPUDescriptorHandleForHeapStart());
    }
    return true;
}
