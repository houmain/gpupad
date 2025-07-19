/*
RenderTargetHelper

Simple helper class for Direct3D 12 that accompanies an article on my blog:

https://asawicki.info/news_1772_secrets_of_direct3d_12_do_rtv_and_dsv_descriptors_make_any_sense

Author:  Adam Sawicki - http://asawicki.info
Version: 1.0.0, 2023-11-12
License: Public Domain
*/
#include "RenderTargetHelper.hpp"

HRESULT RenderTargetHelper::Init(ID3D12Device* pDevice)
{
    m_pDevice = pDevice;
    m_RTVIncrementSize = pDevice->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);

    D3D12_DESCRIPTOR_HEAP_DESC RTVDesc = {};
    RTVDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
    RTVDesc.NumDescriptors = D3D12_SIMULTANEOUS_RENDER_TARGET_COUNT;
    HRESULT hr = pDevice->CreateDescriptorHeap(&RTVDesc, IID_PPV_ARGS(&m_pRTVHeap));

    if(SUCCEEDED(hr))
    {
        m_RTVHandle = m_pRTVHeap->GetCPUDescriptorHandleForHeapStart();

        D3D12_DESCRIPTOR_HEAP_DESC DSVDesc = {};
        DSVDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_DSV;
        DSVDesc.NumDescriptors = 1;
        hr = pDevice->CreateDescriptorHeap(&DSVDesc, IID_PPV_ARGS(&m_pDSVHeap));
    }
    if(SUCCEEDED(hr))
    {
        m_DSVHandle = m_pDSVHeap->GetCPUDescriptorHandleForHeapStart();
    }

    if(FAILED(hr))
        Destroy();

    return hr;
}

void RenderTargetHelper::Destroy()
{
    m_DSVHandle = {};
    m_RTVHandle = {};
    m_pDevice = NULL;

    if(m_pDSVHeap)
    {
        m_pDSVHeap->Release();
        m_pDSVHeap = NULL;
    }
    if(m_pRTVHeap)
    {
        m_pRTVHeap->Release();
        m_pRTVHeap = NULL;
    }
}

void RenderTargetHelper::OMSetRenderTargets(
    ID3D12GraphicsCommandList* pCommandList,
    DWORD NumRenderTargets,
    ID3D12Resource* const* ppRTVResources,
    const D3D12_RENDER_TARGET_VIEW_DESC* pRTVDescs,
    ID3D12Resource* pDSVResource,
    const D3D12_DEPTH_STENCIL_VIEW_DESC* pDSVDesc)
{
    D3D12_CPU_DESCRIPTOR_HANDLE CurrRTVHandle = m_RTVHandle;
    for(DWORD i = 0; i < NumRenderTargets; ++i)
    {
        m_pDevice->CreateRenderTargetView(
            ppRTVResources[i],
            pRTVDescs ? pRTVDescs + i : NULL,
            CurrRTVHandle);
        CurrRTVHandle.ptr += m_RTVIncrementSize;
    }
    
    const D3D12_CPU_DESCRIPTOR_HANDLE* DSVHandle = NULL;
    if(pDSVResource)
    {
        m_pDevice->CreateDepthStencilView(pDSVResource, pDSVDesc, m_DSVHandle);
        DSVHandle = &m_DSVHandle;
    }

    pCommandList->OMSetRenderTargets(NumRenderTargets, &m_RTVHandle, TRUE, DSVHandle);
}

void RenderTargetHelper::ClearRenderTargetView(
    ID3D12GraphicsCommandList* pCommandList,
    ID3D12Resource* pRTVResource,
    const D3D12_RENDER_TARGET_VIEW_DESC* pRTVDesc,
    const FLOAT ColorRGBA[4],
    UINT NumRects,
    const D3D12_RECT* pRects)
{
    m_pDevice->CreateRenderTargetView(pRTVResource, pRTVDesc, m_RTVHandle);
    pCommandList->ClearRenderTargetView(m_RTVHandle, ColorRGBA, NumRects, pRects);
}

void RenderTargetHelper::ClearDepthStencilView(
    ID3D12GraphicsCommandList* pCommandList,
    ID3D12Resource* pDSVResource,
    const D3D12_DEPTH_STENCIL_VIEW_DESC* pDSVDesc,
    D3D12_CLEAR_FLAGS ClearFlags,
    FLOAT Depth,
    UINT8 Stencil,
    UINT NumRects,
    const D3D12_RECT* pRects)
{
    m_pDevice->CreateDepthStencilView(pDSVResource, pDSVDesc, m_DSVHandle);
    pCommandList->ClearDepthStencilView(m_DSVHandle, ClearFlags, Depth, Stencil, NumRects, pRects);
}
