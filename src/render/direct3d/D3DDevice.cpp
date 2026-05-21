#include "D3DDevice.h"

#if defined(_WIN32)

#  include "MessageList.h"

D3DDevice::D3DDevice() : Device(Type::Direct3D) { }

D3DDevice::~D3DDevice()
{
    shutdown();
}

bool D3DDevice::initialize(const AdapterIdentity &adapterIdentity)
try {
    const auto ThrowIfFailed = [](HRESULT hr) {
        if (FAILED(hr))
            throw std::runtime_error("failed");
    };

#  if !defined(NDEBUG)
    auto debugController = ComPtr<ID3D12Debug>();
    ThrowIfFailed(D3D12GetDebugInterface(IID_PPV_ARGS(&debugController)));
    debugController->EnableDebugLayer();
#  endif

    ThrowIfFailed(CreateDXGIFactory1(IID_PPV_ARGS(&mDxgiFactory)));

    mAdapter = [&]() -> IDXGIAdapter * {
        auto i = UINT{};
        auto adapter = std::add_pointer_t<IDXGIAdapter>{};
        while (mDxgiFactory->EnumAdapters(i, &adapter)
            != DXGI_ERROR_NOT_FOUND) {
            auto desc = DXGI_ADAPTER_DESC{};
            adapter->GetDesc(&desc);
            const auto matchesLuid = !std::memcmp(&desc.AdapterLuid,
                &adapterIdentity.deviceLUID, sizeof(LUID));
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
    return true;
} catch (const std::exception &) {
    mMessages.insert(0, MessageType::Direct3DNotAvailable);
    shutdown();
    return false;
}

void D3DDevice::shutdown()
{
    mRenderTargetHelper.Destroy();
    mQueue.Reset();
    mDevice.Reset();
    mAdapter = {};
    mDxgiFactory.Reset();
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
