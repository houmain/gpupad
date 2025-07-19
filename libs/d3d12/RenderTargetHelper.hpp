/*
RenderTargetHelper

Simple helper class for Direct3D 12 that accompanies an article on my blog:

https://asawicki.info/news_1772_secrets_of_direct3d_12_do_rtv_and_dsv_descriptors_make_any_sense

Author:  Adam Sawicki - http://asawicki.info
Version: 1.0.0, 2023-11-12
License: Public Domain
*/
#pragma once

#include <d3d12.h>

class RenderTargetHelper
{
public:
    HRESULT Init(ID3D12Device* pDevice);
    void Destroy();
    ~RenderTargetHelper() { Destroy(); }
    
    void OMSetRenderTargets(
        ID3D12GraphicsCommandList* pCommandList,
        DWORD NumRenderTargets,
        ID3D12Resource* const* ppRTVResources,
        const D3D12_RENDER_TARGET_VIEW_DESC* pRTVDescs,
        ID3D12Resource* pDSVResource,
        const D3D12_DEPTH_STENCIL_VIEW_DESC* pDSVDesc);
    void ClearRenderTargetView(
        ID3D12GraphicsCommandList* pCommandList,
        ID3D12Resource* pRTVResource,
        const D3D12_RENDER_TARGET_VIEW_DESC* pRTVDesc,
        const FLOAT ColorRGBA[4],
        UINT NumRects,
        const D3D12_RECT* pRects);
    void ClearDepthStencilView(
        ID3D12GraphicsCommandList* pCommandList,
        ID3D12Resource* pDSVResource,
        const D3D12_DEPTH_STENCIL_VIEW_DESC* pDSVDesc,
        D3D12_CLEAR_FLAGS ClearFlags,
        FLOAT Depth,
        UINT8 Stencil,
        UINT NumRects,
        const D3D12_RECT* pRects);

private:
    ID3D12Device* m_pDevice = NULL;
    UINT m_RTVIncrementSize = 0;
    ID3D12DescriptorHeap* m_pRTVHeap = NULL;
    ID3D12DescriptorHeap* m_pDSVHeap = NULL;
    D3D12_CPU_DESCRIPTOR_HANDLE m_RTVHandle = {};
    D3D12_CPU_DESCRIPTOR_HANDLE m_DSVHandle = {};
};
