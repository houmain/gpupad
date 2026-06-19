#pragma once
#if defined(D3D_ENABLED)

#include "../ShaderCompiler_Microsoft.h"
#include <dxgi1_4.h>
#include "d3dx12.h"
#include "d3d12/RenderTargetHelper.hpp"

#include "D3DRenderSession.h"
#include "D3DEnums.h"
#include "FileCache.h"
#include "FileDialog.h"
#include "MessageList.h"
#include "Singletons.h"

struct D3DContext
{
    ID3D12Device &device;
    ID3D12CommandQueue &queue;
    RenderTargetHelper &renderTargetHelper;
    ComPtr<ID3D12GraphicsCommandList> graphicsCommandList;
    std::vector<ComPtr<ID3D12Resource>> stagingBuffers;
    const UINT descriptorSize;
};

#endif // D3D_ENABLED
