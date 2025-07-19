
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
        // TODO:
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
    : mItemId(itemId)
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

    mTarget = target;
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

    createRootSignature(context);
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

    createRootSignature(context);
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

        const auto name = QString(paramDesc.SemanticName);
        const auto attribute =
            (mVertexStream ? mVertexStream->findAttribute(name) : nullptr);
        if (!attribute || !attribute->buffer) {
            mMessages += MessageList::insert(mItemId,
                MessageType::AttributeNotSet, name);
            canRender = false;
            continue;
        }

        inputLayout->push_back(D3D12_INPUT_ELEMENT_DESC{
            .SemanticName = paramDesc.SemanticName,
            .SemanticIndex = paramDesc.SemanticIndex,
            .Format = toDXGIFormat(attribute->type, attribute->count),
            .InputSlot = i,
            .AlignedByteOffset = static_cast<UINT>(attribute->offset),
            .InputSlotClass = (attribute->divisor > 1
                    ? D3D12_INPUT_CLASSIFICATION_PER_INSTANCE_DATA
                    : D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA),
            .InstanceDataStepRate = static_cast<UINT>(attribute->divisor),
        });
    }
    return canRender;
}

void D3DPipeline::createRootSignature(D3DContext &context)
{
    struct DescriptorTable
    {
        D3D12_SHADER_VISIBILITY visibility;
        D3D12_DESCRIPTOR_RANGE_TYPE rangeType;
        std::vector<CD3DX12_DESCRIPTOR_RANGE> ranges;
    };
    auto descriptorTables = std::vector<DescriptorTable>();
    auto staticSamplers = std::vector<CD3DX12_STATIC_SAMPLER_DESC>();
    for (const auto [stage, reflection] : mProgram.reflection()) {
        const auto visibility = stageToVisibility(stage);
        auto shaderDesc = D3D12_SHADER_DESC{};
        reflection->GetDesc(&shaderDesc);
        for (auto i = 0u; i < shaderDesc.BoundResources; ++i) {
            auto bindDesc = D3D12_SHADER_INPUT_BIND_DESC{};
            reflection->GetResourceBindingDesc(i, &bindDesc);

            if (bindDesc.Type == D3D_SIT_SAMPLER) {
                auto &sampler = staticSamplers.emplace_back(bindDesc.BindPoint);
                sampler.RegisterSpace = bindDesc.Space;

                if (auto binding = find(mBindings.samplers, bindDesc.Name)) {
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

            if (descriptorTables.empty()
                || descriptorTables.back().visibility != visibility
                || descriptorTables.back().rangeType != rangeType)
                descriptorTables.push_back({ visibility, rangeType });

            descriptorTables.back().ranges.emplace_back().Init(rangeType,
                bindDesc.BindCount, bindDesc.BindPoint, bindDesc.Space);
            ++mDescriptorCount;
        }
    }

    auto rootParameters = std::vector<CD3DX12_ROOT_PARAMETER>();
    for (auto &descriptorTable : descriptorTables) {
        rootParameters.emplace_back().InitAsDescriptorTable(
            static_cast<UINT>(descriptorTable.ranges.size()),
            descriptorTable.ranges.data(), descriptorTable.visibility);
        mDescriptorTableEntries.push_back(
            static_cast<int>(descriptorTable.ranges.size()));
    }

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
}

void D3DPipeline::createGlobalConstantBuffers(D3DContext &context)
{
    for (const auto [stage, reflection] : mProgram.reflection()) {
        auto shaderDesc = D3D12_SHADER_DESC{};
        reflection->GetDesc(&shaderDesc);
        for (auto i = 0u; i < shaderDesc.BoundResources; ++i) {
            auto bindDesc = D3D12_SHADER_INPUT_BIND_DESC{};
            reflection->GetResourceBindingDesc(i, &bindDesc);
            if (bindDesc.Type == D3D_SIT_CBUFFER
                && isGlobalConstantsBufferName(bindDesc.Name)) {

                const auto cbuffer =
                    reflection->GetConstantBufferByIndex(bindDesc.BindPoint);
                auto cbufferDesc = D3D12_SHADER_BUFFER_DESC{};
                cbuffer->GetDesc(&cbufferDesc);

                auto size = UINT{ 1 };
                for (auto v = 0u; v < cbufferDesc.Variables; ++v) {
                    const auto var = cbuffer->GetVariableByIndex(v);
                    auto varDesc = D3D12_SHADER_VARIABLE_DESC{};
                    var->GetDesc(&varDesc);
                    size = std::max(size, varDesc.StartOffset + varDesc.Size);
                }
                mGlobalConstantBuffers[stage] =
                    std::make_unique<D3DBuffer>(size);
            }
        }
    }
}

void D3DPipeline::updateGlobalConstantBuffers(D3DContext &context,
    ScriptEngine &scriptEngine)
{
    for (const auto [stage, reflection] : mProgram.reflection()) {
        auto shaderDesc = D3D12_SHADER_DESC{};
        reflection->GetDesc(&shaderDesc);
        for (auto i = 0u; i < shaderDesc.BoundResources; ++i) {
            auto bindDesc = D3D12_SHADER_INPUT_BIND_DESC{};
            reflection->GetResourceBindingDesc(i, &bindDesc);
            if (bindDesc.Type == D3D_SIT_CBUFFER
                && isGlobalConstantsBufferName(bindDesc.Name)) {

                auto buffer = getGlobalConstantBuffer(stage);
                auto &data = buffer->writableData();

                const auto cbuffer =
                    reflection->GetConstantBufferByIndex(bindDesc.BindPoint);
                auto cbufferDesc = D3D12_SHADER_BUFFER_DESC{};
                cbuffer->GetDesc(&cbufferDesc);
                for (auto v = 0u; v < cbufferDesc.Variables; ++v) {
                    const auto var = cbuffer->GetVariableByIndex(v);
                    auto varDesc = D3D12_SHADER_VARIABLE_DESC{};
                    var->GetDesc(&varDesc);

                    const auto name = QString(varDesc.Name);
                    if (auto uniformBinding = find(mBindings.uniforms, name)) {
                        mUsedItems += uniformBinding->bindingItemId;

                        // TODO:
                        auto values = getValues<float>(scriptEngine,
                            uniformBinding->values, 0, 16, mItemId);

                        Q_ASSERT(varDesc.StartOffset + varDesc.Size
                            <= data.size());
                        std::memcpy(data.data() + varDesc.StartOffset,
                            values.data(), varDesc.Size);
                    } else {
                        mMessages += MessageList::insert(mItemId,
                            MessageType::UniformNotSet, name);
                    }
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
    if (!mDescriptorCount)
        return;

    const auto descriptorHeapDesc = D3D12_DESCRIPTOR_HEAP_DESC{
        .Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV,
        .NumDescriptors = mDescriptorCount,
        .Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE,
        .NodeMask = 0,
    };
    AssertIfFailed(context.device.CreateDescriptorHeap(&descriptorHeapDesc,
        IID_PPV_ARGS(&mDescriptorHeap)));
}

bool D3DPipeline::setDescriptors(D3DContext &context)
{
    const auto descriptorSize = context.device.GetDescriptorHandleIncrementSize(
        D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
    auto descriptor = CD3DX12_CPU_DESCRIPTOR_HANDLE{
        mDescriptorHeap->GetCPUDescriptorHandleForHeapStart()
    };

    auto canRender = true;
    for (const auto [stage, reflection] : mProgram.reflection()) {
        auto shaderDesc = D3D12_SHADER_DESC{};
        reflection->GetDesc(&shaderDesc);
        for (auto i = 0u; i < shaderDesc.BoundResources; ++i) {
            auto bindDesc = D3D12_SHADER_INPUT_BIND_DESC{};
            reflection->GetResourceBindingDesc(i, &bindDesc);
            const auto name = QString(bindDesc.Name);

            if (bindDesc.Type == D3D_SIT_CBUFFER) {
                auto buffer = std::add_pointer_t<D3DBuffer>{};
                if (auto bufferBinding = find(mBindings.buffers, name)) {
                    buffer = static_cast<D3DBuffer *>(bufferBinding->buffer);
                    mUsedItems += bufferBinding->bindingItemId;
                    mUsedItems += bufferBinding->blockItemId;
                } else if (isGlobalConstantsBufferName(name)) {
                    buffer = getGlobalConstantBuffer(stage);
                }

                if (!buffer) {
                    mMessages += MessageList::insert(mItemId,
                        MessageType::BufferNotSet, name);
                    canRender = false;
                    continue;
                }
                buffer->prepareConstantBufferView(context);

                const auto cbvDesc = D3D12_CONSTANT_BUFFER_VIEW_DESC{
                    .BufferLocation = buffer->getDeviceAddress(),
                    .SizeInBytes = buffer->alignedSize(),
                };
                context.device.CreateConstantBufferView(&cbvDesc, descriptor);
                descriptor.Offset(1, descriptorSize);
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
                texture->prepareShaderResourceView(context);

                const auto srvDesc = texture->shaderResourceViewDesc();
                context.device.CreateShaderResourceView(texture->resource(),
                    &srvDesc, descriptor);
                descriptor.Offset(1, descriptorSize);

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
                texture->prepareUnorderedAccessView(context);

                const auto uavDesc = texture->unorderedAccessViewDesc();
                context.device.CreateUnorderedAccessView(texture->resource(),
                    nullptr, &uavDesc, descriptor);
                descriptor.Offset(1, descriptorSize);

            } else if (bindDesc.Type == D3D_SIT_SAMPLER) {
                // nothing to do
            } else {
                Q_ASSERT(!"binding type not handled");
            }
        }
    }
    return canRender;
}

void D3DPipeline::setBindings(Bindings &&bindings)
{
    mBindings = std::move(bindings);
}

void D3DPipeline::bindGraphics(D3DContext &context, ScriptEngine &scriptEngine)
{
    context.graphicsCommandList->SetPipelineState(mPipelineState.Get());
    context.graphicsCommandList->SetGraphicsRootSignature(mRootSignature.Get());

    if (mTarget)
        mTarget->bind(context);
    if (mVertexStream)
        mVertexStream->bind(context);

    //context.graphicsCommandList->IASetIndexBuffer(&mBoxGeo->IndexBufferView());

    if (mDescriptorHeap) {
        if (!setDescriptors(context))
            return;

        updateGlobalConstantBuffers(context, scriptEngine);

        auto descriptorHeaps = mDescriptorHeap.Get();
        context.graphicsCommandList->SetDescriptorHeaps(1, &descriptorHeaps);

        const auto descriptorSize =
            context.device.GetDescriptorHandleIncrementSize(
                D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
        auto descriptor = CD3DX12_GPU_DESCRIPTOR_HANDLE{
            mDescriptorHeap->GetGPUDescriptorHandleForHeapStart()
        };
        for (auto i = 0u; i < mDescriptorTableEntries.size(); ++i) {
            context.graphicsCommandList->SetGraphicsRootDescriptorTable(i,
                descriptor);
            descriptor.Offset(mDescriptorTableEntries[i], descriptorSize);
        }
    }
}

void D3DPipeline::bindCompute(D3DContext &context, ScriptEngine &scriptEngine)
{
    context.graphicsCommandList->SetPipelineState(mPipelineState.Get());
    context.graphicsCommandList->SetComputeRootSignature(mRootSignature.Get());

    if (mDescriptorHeap) {
        if (!setDescriptors(context))
            return;

        updateGlobalConstantBuffers(context, scriptEngine);

        auto descriptorHeaps = mDescriptorHeap.Get();
        context.graphicsCommandList->SetDescriptorHeaps(1, &descriptorHeaps);

        const auto descriptorSize =
            context.device.GetDescriptorHandleIncrementSize(
                D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
        auto descriptor = CD3DX12_GPU_DESCRIPTOR_HANDLE{
            mDescriptorHeap->GetGPUDescriptorHandleForHeapStart()
        };
        for (auto i = 0u; i < mDescriptorTableEntries.size(); ++i) {
            context.graphicsCommandList->SetComputeRootDescriptorTable(i,
                descriptor);
            descriptor.Offset(mDescriptorTableEntries[i], descriptorSize);
        }
    }
}
