#pragma once
#if defined(D3D_ENABLED)

#  include "render/AdapterIdentity.h"
#  include "render/Device.h"
#  include "render/ShaderCompiler_Microsoft.h"
#  include "MessageList.h"
#  include "d3dx12.h"
#  include "d3d12/RenderTargetHelper.hpp"
#  include <dxgi1_4.h>

struct ID3D12CommandQueue;
struct ID3D12Device;

class D3DDevice final : public Device
{
public:
    D3DDevice();
    ~D3DDevice() override;

    bool initialize(const AdapterIdentity &adapterIdentity) override;

    ID3D12Device &device();
    ID3D12CommandQueue &queue();
    RenderTargetHelper &renderTargetHelper();

private:
    ComPtr<IDXGIFactory4> mDxgiFactory;
    IDXGIAdapter *mAdapter{};
    ComPtr<ID3D12Device> mDevice;
    ComPtr<ID3D12CommandQueue> mQueue;
    RenderTargetHelper mRenderTargetHelper;
    MessagePtrSet mMessages;
};

#endif // defined(D3D_ENABLED)
