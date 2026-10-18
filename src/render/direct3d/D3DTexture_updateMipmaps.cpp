#include "D3DTexture.h"

#include <d3dcompiler.h>

namespace {
    const auto updateMipmapsShaderCommon = R"(
float srgb(float value) {
    value = saturate(value);
    return value <= 0.0031308 ? value * 12.92
        : 1.055 * pow(value, 1.0 / 2.4) - 0.055;
}

float4 encode(float4 value) {
#if ENCODE_SRGB
    value.rgb = float3(srgb(value.r), srgb(value.g), srgb(value.b));
#endif
    return value;
}
)";

    const auto updateMipmapsShader1D = R"(
Texture1DArray<float4> sourceTexture : register(t0);
RWTexture1DArray<float4> destinationTexture : register(u0);
SamplerState linearClampSampler : register(s0);

[numthreads(64, 1, 1)]
void main(uint3 id : SV_DispatchThreadID) {
    uint width, slices;
    destinationTexture.GetDimensions(width, slices);
    if (id.x >= width || id.y >= slices)
        return;
    float location = (id.x + 0.5) / width;
    destinationTexture[id.xy] = encode(sourceTexture.SampleLevel(
        linearClampSampler, float2(location, id.y), 0.0));
}
)";

    const auto updateMipmapsShader2D = R"(
Texture2DArray<float4> sourceTexture : register(t0);
RWTexture2DArray<float4> destinationTexture : register(u0);
SamplerState linearClampSampler : register(s0);

[numthreads(8, 8, 1)]
void main(uint3 id : SV_DispatchThreadID) {
    uint width, height, slices;
    destinationTexture.GetDimensions(width, height, slices);
    if (id.x >= width || id.y >= height || id.z >= slices)
        return;
    float2 location = (id.xy + 0.5) / float2(width, height);
    destinationTexture[id] = encode(sourceTexture.SampleLevel(
        linearClampSampler, float3(location, id.z), 0.0));
}
)";

    const auto updateMipmapsShader3D = R"(
Texture3D<float4> sourceTexture : register(t0);
RWTexture3D<float4> destinationTexture : register(u0);
SamplerState linearClampSampler : register(s0);

[numthreads(4, 4, 4)]
void main(uint3 id : SV_DispatchThreadID) {
    uint width, height, depth;
    destinationTexture.GetDimensions(width, height, depth);
    if (id.x >= width || id.y >= height || id.z >= depth)
        return;
    float3 location = (id + 0.5) / float3(width, height, depth);
    destinationTexture[id] = encode(sourceTexture.SampleLevel(
        linearClampSampler, location, 0.0));
}
)";

    DXGI_FORMAT getMipmapUnorderedAccessFormat(Texture::Format format)
    {
        using TF = Texture::Format;
        switch (format) {
        case TF::SRGB8_Alpha8:    return DXGI_FORMAT_R8G8B8A8_UNORM;
        case TF::SRGB_DXT1:
        case TF::SRGB_Alpha_DXT1:
        case TF::SRGB_Alpha_DXT3:
        case TF::SRGB_Alpha_DXT5:
        case TF::SRGB_BP_UNorm:   return DXGI_FORMAT_UNKNOWN;
        default:                  return toDXGIFormat(format);
        }
    }
} // namespace

bool D3DTexture::updateMipmaps(D3DContext &context)
{
    if (!resource())
        return false;

    if (!mMipmapsInvalidated || levels() <= 1)
        return true;

    if (!createMipmapPipeline(context))
        return false;

    const auto srvFormat = toDXGIShaderResourceFormat(mFormat);
    const auto uavFormat = getMipmapUnorderedAccessFormat(mFormat);
    if (srvFormat == DXGI_FORMAT_UNKNOWN || uavFormat == DXGI_FORMAT_UNKNOWN)
        return false;

    const auto dimensions = mKind.dimensions;
    const auto arraySlices = static_cast<UINT>(
        dimensions == 3 ? 1 : mLayers * (mKind.cubeMap ? 6 : 1));
    const auto mipLevels = static_cast<UINT>(levels());

    resourceBarrier(context, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);

    auto descriptorHeap = mMipmapDescriptorHeap.Get();
    context.graphicsCommandList->SetDescriptorHeaps(1, &descriptorHeap);
    context.graphicsCommandList->SetComputeRootSignature(
        mMipmapRootSignature.Get());
    context.graphicsCommandList->SetPipelineState(mMipmapPipelineState.Get());

    for (auto level = 1u; level < mipLevels; ++level) {
        auto sourceCpuDescriptor = CD3DX12_CPU_DESCRIPTOR_HANDLE(
            mMipmapDescriptorHeap->GetCPUDescriptorHandleForHeapStart(),
            2 * (level - 1), context.descriptorSize);
        auto sourceGpuDescriptor = CD3DX12_GPU_DESCRIPTOR_HANDLE(
            mMipmapDescriptorHeap->GetGPUDescriptorHandleForHeapStart(),
            2 * (level - 1), context.descriptorSize);
        auto destinationCpuDescriptor = sourceCpuDescriptor;
        auto destinationGpuDescriptor = sourceGpuDescriptor;
        destinationCpuDescriptor.Offset(1, context.descriptorSize);
        destinationGpuDescriptor.Offset(1, context.descriptorSize);

        auto srvDesc = D3D12_SHADER_RESOURCE_VIEW_DESC{
            .Format = srvFormat,
            .ViewDimension = (dimensions == 1
                    ? D3D12_SRV_DIMENSION_TEXTURE1DARRAY
                    : dimensions == 2 ? D3D12_SRV_DIMENSION_TEXTURE2DARRAY
                                      : D3D12_SRV_DIMENSION_TEXTURE3D),
            .Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING,
        };
        if (dimensions == 1) {
            srvDesc.Texture1DArray = {
                .MostDetailedMip = level - 1,
                .MipLevels = 1,
                .FirstArraySlice = 0,
                .ArraySize = arraySlices,
            };
        } else if (dimensions == 2) {
            srvDesc.Texture2DArray = {
                .MostDetailedMip = level - 1,
                .MipLevels = 1,
                .FirstArraySlice = 0,
                .ArraySize = arraySlices,
            };
        } else {
            srvDesc.Texture3D = {
                .MostDetailedMip = level - 1,
                .MipLevels = 1,
            };
        }

        auto uavDesc = D3D12_UNORDERED_ACCESS_VIEW_DESC{
            .Format = uavFormat,
            .ViewDimension = (dimensions == 1
                    ? D3D12_UAV_DIMENSION_TEXTURE1DARRAY
                    : dimensions == 2 ? D3D12_UAV_DIMENSION_TEXTURE2DARRAY
                                      : D3D12_UAV_DIMENSION_TEXTURE3D),
        };
        if (dimensions == 1) {
            uavDesc.Texture1DArray = {
                .MipSlice = level,
                .FirstArraySlice = 0,
                .ArraySize = arraySlices,
            };
        } else if (dimensions == 2) {
            uavDesc.Texture2DArray = {
                .MipSlice = level,
                .FirstArraySlice = 0,
                .ArraySize = arraySlices,
            };
        } else {
            uavDesc.Texture3D = {
                .MipSlice = level,
                .FirstWSlice = 0,
                .WSize = static_cast<UINT>(mData.getLevelDepth(level)),
            };
        }

        context.device.CreateShaderResourceView(resource(), &srvDesc,
            sourceCpuDescriptor);
        context.device.CreateUnorderedAccessView(resource(), nullptr, &uavDesc,
            destinationCpuDescriptor);

        auto barriers = std::vector<D3D12_RESOURCE_BARRIER>();
        barriers.reserve(arraySlices);
        for (auto arraySlice = 0u; arraySlice < arraySlices; ++arraySlice)
            barriers.push_back(CD3DX12_RESOURCE_BARRIER::Transition(resource(),
                D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE,
                D3D12_RESOURCE_STATE_UNORDERED_ACCESS,
                D3D12CalcSubresource(level, arraySlice, 0, mipLevels,
                    arraySlices)));
        context.graphicsCommandList->ResourceBarrier(
            static_cast<UINT>(barriers.size()), barriers.data());

        context.graphicsCommandList->SetComputeRootDescriptorTable(0,
            sourceGpuDescriptor);
        context.graphicsCommandList->SetComputeRootDescriptorTable(1,
            destinationGpuDescriptor);

        const auto width = static_cast<UINT>(mData.getLevelWidth(level));
        const auto height = static_cast<UINT>(mData.getLevelHeight(level));
        const auto depth = static_cast<UINT>(mData.getLevelDepth(level));
        if (dimensions == 1) {
            context.graphicsCommandList->Dispatch((width + 63) / 64,
                arraySlices, 1);
        } else if (dimensions == 2) {
            context.graphicsCommandList->Dispatch((width + 7) / 8,
                (height + 7) / 8, arraySlices);
        } else {
            context.graphicsCommandList->Dispatch((width + 3) / 4,
                (height + 3) / 4, (depth + 3) / 4);
        }

        for (auto &barrier : barriers)
            std::swap(barrier.Transition.StateBefore,
                barrier.Transition.StateAfter);
        context.graphicsCommandList->ResourceBarrier(
            static_cast<UINT>(barriers.size()), barriers.data());
    }
    mMipmapsInvalidated = false;
    return true;
}

bool D3DTexture::createMipmapPipeline(D3DContext &context)
{
    if (mMipmapPipelineState)
        return true;

    const auto sampleType = getTextureSampleType(mFormat);
    if (sampleType != TextureSampleType::Normalized
        && sampleType != TextureSampleType::Normalized_sRGB
        && sampleType != TextureSampleType::Float)
        return false;

    auto formatSupport = D3D12_FEATURE_DATA_FORMAT_SUPPORT{
        .Format = getMipmapUnorderedAccessFormat(mFormat),
    };
    if (formatSupport.Format == DXGI_FORMAT_UNKNOWN
        || FAILED(
            context.device.CheckFeatureSupport(D3D12_FEATURE_FORMAT_SUPPORT,
                &formatSupport, sizeof(formatSupport)))
        || !(formatSupport.Support2 & D3D12_FORMAT_SUPPORT2_UAV_TYPED_STORE))
        return false;

    const auto source = std::string("#define ENCODE_SRGB ")
        + (sampleType == TextureSampleType::Normalized_sRGB ? "1\n" : "0\n")
        + updateMipmapsShaderCommon
        + (mKind.dimensions == 1        ? updateMipmapsShader1D
                : mKind.dimensions == 2 ? updateMipmapsShader2D
                                        : updateMipmapsShader3D);
    auto shader = ComPtr<ID3DBlob>();
    auto errors = ComPtr<ID3DBlob>();
    if (FAILED(D3DCompile(source.data(), source.size(), "GenerateMipmaps",
            nullptr, nullptr, "main", "cs_5_1", D3DCOMPILE_DEBUG, 0, &shader,
            &errors))) {
        const auto message = (errors
                ? QString::fromUtf8(
                      static_cast<const char *>(errors->GetBufferPointer()),
                      errors->GetBufferSize())
                : QString());
        mMessages.insert(mItemId, MessageType::ShaderError, message);
        return false;
    }

    auto ranges = std::array<CD3DX12_DESCRIPTOR_RANGE, 2>{ };
    ranges[0].Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 0);
    ranges[1].Init(D3D12_DESCRIPTOR_RANGE_TYPE_UAV, 1, 0);
    auto rootParameters = std::array<CD3DX12_ROOT_PARAMETER, 2>{ };
    rootParameters[0].InitAsDescriptorTable(1, &ranges[0]);
    rootParameters[1].InitAsDescriptorTable(1, &ranges[1]);
    const auto sampler = D3D12_STATIC_SAMPLER_DESC{
        .Filter = D3D12_FILTER_MIN_MAG_LINEAR_MIP_POINT,
        .AddressU = D3D12_TEXTURE_ADDRESS_MODE_CLAMP,
        .AddressV = D3D12_TEXTURE_ADDRESS_MODE_CLAMP,
        .AddressW = D3D12_TEXTURE_ADDRESS_MODE_CLAMP,
        .MipLODBias = 0.0f,
        .MaxAnisotropy = 1,
        .ComparisonFunc = D3D12_COMPARISON_FUNC_ALWAYS,
        .BorderColor = D3D12_STATIC_BORDER_COLOR_TRANSPARENT_BLACK,
        .MinLOD = 0.0f,
        .MaxLOD = 0.0f,
        .ShaderRegister = 0,
        .RegisterSpace = 0,
        .ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL,
    };
    const auto rootSignatureDesc =
        CD3DX12_ROOT_SIGNATURE_DESC(static_cast<UINT>(rootParameters.size()),
            rootParameters.data(), 1, &sampler);
    auto rootSignatureBlob = ComPtr<ID3DBlob>();
    if (FAILED(D3D12SerializeRootSignature(&rootSignatureDesc,
            D3D_ROOT_SIGNATURE_VERSION_1, &rootSignatureBlob, &errors))
        || FAILED(context.device.CreateRootSignature(0,
            rootSignatureBlob->GetBufferPointer(),
            rootSignatureBlob->GetBufferSize(),
            IID_PPV_ARGS(&mMipmapRootSignature)))) {
        mMessages.insert(mItemId, MessageType::CreatingPipelineFailed);
        return false;
    }

    const auto pipelineDesc = D3D12_COMPUTE_PIPELINE_STATE_DESC{
        .pRootSignature = mMipmapRootSignature.Get(),
        .CS = {
            .pShaderBytecode = shader->GetBufferPointer(),
            .BytecodeLength = shader->GetBufferSize(),
        },
    };
    if (FAILED(context.device.CreateComputePipelineState(&pipelineDesc,
            IID_PPV_ARGS(&mMipmapPipelineState)))) {
        mMessages.insert(mItemId, MessageType::CreatingPipelineFailed);
        return false;
    }

    const auto heapDesc = D3D12_DESCRIPTOR_HEAP_DESC{
        .Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV,
        .NumDescriptors = 2 * static_cast<UINT>(levels() - 1),
        .Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE,
    };
    if (FAILED(context.device.CreateDescriptorHeap(&heapDesc,
            IID_PPV_ARGS(&mMipmapDescriptorHeap)))) {
        mMessages.insert(mItemId, MessageType::CreatingPipelineFailed);
        return false;
    }
    return true;
}
