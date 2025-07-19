#pragma once

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <wrl.h>
#include <dxgi1_4.h>
#include <d3d12shader.h>
#include "d3dx12.h"
#include "d3d12/RenderTargetHelper.hpp"

using Microsoft::WRL::ComPtr;

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
};

inline void AssertIfFailed(HRESULT hr)
{
    Q_ASSERT(SUCCEEDED(hr));
}
