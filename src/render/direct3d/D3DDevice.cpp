#include "D3DDevice.h"

#if defined(_WIN32)

#  include "message/MessageList.h"

namespace {
    QString queryFailure(HRESULT result)
    {
        return QStringLiteral("unavailable (HRESULT 0x%1)")
            .arg(static_cast<quint32>(result), 8, 16, QChar('0'));
    }

    QString shaderModelName(D3D_SHADER_MODEL model)
    {
        const auto value = static_cast<UINT>(model);
        return QStringLiteral("%1.%2").arg(value >> 4).arg(value & 0xf);
    }

    QString queryRayTracingTier(ID3D12Device &device)
    {
        auto options = D3D12_FEATURE_DATA_D3D12_OPTIONS5{ };
        const auto result = device.CheckFeatureSupport(
            D3D12_FEATURE_D3D12_OPTIONS5, &options, sizeof(options));
        if (FAILED(result))
            return queryFailure(result);
        if (options.RaytracingTier == D3D12_RAYTRACING_TIER_NOT_SUPPORTED)
            return QStringLiteral("not supported");
        const auto tier = static_cast<UINT>(options.RaytracingTier);
        return QStringLiteral("%1.%2 (value %3)")
            .arg(tier / 10)
            .arg(tier % 10)
            .arg(tier);
    }

    QString queryShaderModel(ID3D12Device &device)
    {
        // retry older models when the runtime does not recognize the SDK ceiling
        for (auto requested = static_cast<UINT>(D3D_HIGHEST_SHADER_MODEL);;
            requested =
                (requested == D3D_SHADER_MODEL_6_0 ? D3D_SHADER_MODEL_5_1
                                                   : requested - 1)) {
            auto model = D3D12_FEATURE_DATA_SHADER_MODEL{
                static_cast<D3D_SHADER_MODEL>(requested)
            };
            const auto result = device.CheckFeatureSupport(
                D3D12_FEATURE_SHADER_MODEL, &model, sizeof(model));
            if (SUCCEEDED(result))
                return shaderModelName(model.HighestShaderModel);
            if (result != E_INVALIDARG || requested == D3D_SHADER_MODEL_5_1)
                return queryFailure(result);
        }
    }

#  if !defined(NDEBUG)
    const bool sDebugLayerEnabled = [] {
        auto debug = ComPtr<ID3D12Debug>();
        if (FAILED(D3D12GetDebugInterface(IID_PPV_ARGS(&debug))))
            return false;
        debug->EnableDebugLayer();
        return true;
    }();
#  endif
} // namespace

D3DDevice::D3DDevice(const AdapterIdentity &adapterIdentity)
    : Device(Type::Direct3D)
    , mAdapterIdentity(adapterIdentity)
{
}

D3DDevice::~D3DDevice()
{
    mRenderTargetHelper.Destroy();
    mQueue.Reset();
    mDevice.Reset();
    mAdapter = {};
    mDxgiFactory.Reset();
}

bool D3DDevice::initialize()
try {
    const auto ThrowIfFailed = [](HRESULT hr) {
        if (FAILED(hr))
            throw std::runtime_error(qUtf8Printable("HRESULT: "
                + QString::number(static_cast<unsigned long>(hr), 16)));
    };

    ThrowIfFailed(CreateDXGIFactory1(IID_PPV_ARGS(&mDxgiFactory)));

    mAdapter = [&]() -> IDXGIAdapter * {
        auto i = UINT{};
        auto adapter = std::add_pointer_t<IDXGIAdapter>{};
        while (mDxgiFactory->EnumAdapters(i, &adapter)
            != DXGI_ERROR_NOT_FOUND) {
            auto desc = DXGI_ADAPTER_DESC{};
            adapter->GetDesc(&desc);
            const auto matchesLuid = !std::memcmp(&desc.AdapterLuid,
                &mAdapterIdentity.deviceLUID, sizeof(LUID));
            if (matchesLuid)
                return adapter;
            ++i;
        }
        return nullptr;
    }();

    ThrowIfFailed(D3D12CreateDevice(mAdapter, D3D_FEATURE_LEVEL_11_0,
        IID_PPV_ARGS(&mDevice)));

    auto queueDesc = D3D12_COMMAND_QUEUE_DESC{};
    queueDesc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;
    queueDesc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;
    ThrowIfFailed(
        mDevice->CreateCommandQueue(&queueDesc, IID_PPV_ARGS(&mQueue)));

    ThrowIfFailed(mRenderTargetHelper.Init(mDevice.Get()));
    reportCapabilities();
    return true;
} catch (const std::exception &ex) {
    mMessages.insert(MessageType::Direct3DNotAvailable, ex.what());
    return false;
}

void D3DDevice::reportCapabilities()
{
    // device support is independent of compiler and pipeline support
    mMessages.insert(MessageType::DeviceInfo,
        QStringLiteral("Direct3D12 capabilities\nDXR tier (OPTIONS5): %1\n"
                       "Highest device shader model: %2 (query ceiling %3)")
            .arg(queryRayTracingTier(*mDevice.Get()),
                queryShaderModel(*mDevice.Get()),
                shaderModelName(D3D_HIGHEST_SHADER_MODEL)));
}

ID3D12Device &D3DDevice::device()
{
    Q_ASSERT(mDevice);
    return *mDevice.Get();
}

ID3D12CommandQueue &D3DDevice::queue()
{
    Q_ASSERT(mQueue);
    return *mQueue.Get();
}

RenderTargetHelper &D3DDevice::renderTargetHelper()
{
    return mRenderTargetHelper;
}

#endif // defined(_WIN32)
