#include "DX12RenderTargetManager.h"

#include <cassert>
#include <d3d12.h>
#include <codecvt>

#include "DX12CommandList.h"
#include "DX12DescriptorHeap.h"
#include "DX12Utils.h"
#include "../RHIResourceFactoryImpl.hpp"

DX12RenderTargetManager::DX12RenderTargetManager()
    : m_maxRenderTargetCount(0)
    , m_rtvDescriptorHeap(nullptr)
    , m_dsvDescriptorHeap(nullptr)
{
}

DX12RenderTargetManager::~DX12RenderTargetManager()
{

}

bool DX12RenderTargetManager::InitRenderTargetManager(IRHIDevice& device, size_t maxRenderTargetCount)
{
    auto* dxDevice = dynamic_cast<DX12Device&>(device).GetDevice();

    m_rtvDescriptorHeap = RHIResourceFactory::CreateRHIResource<IRHIDescriptorHeap>();
    m_rtvDescriptorHeap->InitDescriptorHeap(device, {static_cast<unsigned>(maxRenderTargetCount), RHIDescriptorHeapType::RTV, false});

    m_dsvDescriptorHeap = RHIResourceFactory::CreateRHIResource<IRHIDescriptorHeap>();
    m_dsvDescriptorHeap->InitDescriptorHeap(device, {static_cast<unsigned>(maxRenderTargetCount), RHIDescriptorHeapType::DSV, false});
    
    m_maxRenderTargetCount = maxRenderTargetCount;
    
    return true;
}

std::shared_ptr<IRHIRenderTarget> DX12RenderTargetManager::CreateRenderTarget(IRHIDevice& device, RHIRenderTargetType type, RHIDataFormat format, const IRHIRenderTargetDesc& desc)
{
    auto* dxDevice = dynamic_cast<DX12Device&>(device).GetDevice();
    
    // Check has enough space for descriptor?
    D3D12_RESOURCE_FLAGS flags = D3D12_RESOURCE_FLAG_NONE;
    size_t handleCount = 0;
    D3D12_CPU_DESCRIPTOR_HANDLE handle = {0};
    D3D12_RESOURCE_STATES initialState = D3D12_RESOURCE_STATE_COMMON;
    DXGI_FORMAT dxFormat = DX12ConverterUtils::ConvertToDXGIFormat(format);
    D3D12_CLEAR_VALUE dxClearValue{};
    dxClearValue.Format = dxFormat;
    
    switch (type)
    { 
    case RHIRenderTargetType::RTV:
        {
            if(m_rtvHandles.size() >= m_maxRenderTargetCount)
            {
                return nullptr;
            }
            
            handleCount = m_rtvHandles.size();
            flags = D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET;
            const UINT incSize = dxDevice->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
            auto* dxDescriptorHeap = dynamic_cast<DX12DescriptorHeap&>(*m_rtvDescriptorHeap).GetDescriptorHeap();
            handle = dxDescriptorHeap->GetCPUDescriptorHandleForHeapStart();
            handle.ptr += handleCount * incSize;
            initialState = D3D12_RESOURCE_STATE_RENDER_TARGET;
            memcpy(dxClearValue.Color, &desc.clearValue.clearColor, sizeof(desc.clearValue.clearColor));
        }
        break;
        
    case RHIRenderTargetType::DSV:
        {
            if(m_dsvHandles.size() >= m_maxRenderTargetCount)
            {
                return nullptr;
            }
            
            handleCount = m_dsvHandles.size();
            flags = D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL;
            const UINT incSize = dxDevice->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_DSV);
            auto* dxDescriptorHeap = dynamic_cast<DX12DescriptorHeap&>(*m_dsvDescriptorHeap).GetDescriptorHeap();
            handle = dxDescriptorHeap->GetCPUDescriptorHandleForHeapStart();
            handle.ptr += handleCount * incSize;
            initialState = D3D12_RESOURCE_STATE_DEPTH_WRITE;
            dxClearValue.DepthStencil.Depth = desc.clearValue.clearDS.clearDepth;
            dxClearValue.DepthStencil.Stencil = desc.clearValue.clearDS.clearStencilValue;
        }
        break;
        
    case RHIRenderTargetType::Unknown:
        // Fatal error!
        assert(false);
        return nullptr;
    }

    if (desc.isUAV)
    {
        flags |= D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;
    }

    D3D12_HEAP_PROPERTIES heapProperties = {};
    heapProperties.Type = D3D12_HEAP_TYPE_DEFAULT;
    heapProperties.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;

    D3D12_RESOURCE_DESC resourceDesc = {};
    resourceDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
    resourceDesc.Format = dxFormat;
    resourceDesc.Width = desc.width;
    resourceDesc.Height = desc.height;
    resourceDesc.DepthOrArraySize = 1;
    resourceDesc.MipLevels = 1;
    resourceDesc.Alignment = 0;
    resourceDesc.SampleDesc.Count = 1;
    resourceDesc.SampleDesc.Quality = 0;
    resourceDesc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
    resourceDesc.Flags = flags;

    ID3D12Resource* resource = nullptr;
    THROW_IF_FAILED(dxDevice->CreateCommittedResource(&heapProperties,
                                                  D3D12_HEAP_FLAG_NONE,
                                                  &resourceDesc,
                                                  initialState,
                                                  &dxClearValue,
                                                  IID_PPV_ARGS(&resource)))

    if (!desc.name.empty())
    {
        std::wstring_convert<std::codecvt_utf8<wchar_t>> converter;
        const std::wstring name = converter.from_bytes(desc.name);
        resource->SetName(name.c_str());
    }

    std::shared_ptr<IRHIRenderTarget> renderTarget = RHIResourceFactory::CreateRHIResource<IRHIRenderTarget>();
    renderTarget->SetRenderTargetType(type);
    renderTarget->SetRenderTargetFormat(format);
    auto* dxRenderTarget = dynamic_cast<DX12RenderTarget*>(renderTarget.get());
    dxRenderTarget->SetRenderTarget(resource, true);
    dxRenderTarget->SetClearValue(dxClearValue);

    switch (type) {
        case RHIRenderTargetType::RTV:
            {
                dxDevice->CreateRenderTargetView(resource, nullptr, handle);
                m_rtvHandles[renderTarget->GetRenderTargetId()] = handle;
            }
            break;
        case RHIRenderTargetType::DSV:
            {
                D3D12_DEPTH_STENCIL_VIEW_DESC viewDescription = {};
                viewDescription.Format = dxFormat;
                viewDescription.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2D;
                
                dxDevice->CreateDepthStencilView(resource, &viewDescription, handle);
                m_dsvHandles[renderTarget->GetRenderTargetId()] = handle;
            }
            break;
    }
    
    return renderTarget;
}

std::vector<std::shared_ptr<IRHIRenderTarget>> DX12RenderTargetManager::CreateRenderTargetFromSwapChain(
    IRHIDevice& device, IRHISwapChain& swapChain, RHIRenderTargetClearValue clearValue)
{
    auto* dxDevice = dynamic_cast<DX12Device&>(device).GetDevice();
    auto* dxSwapChain = dynamic_cast<DX12SwapChain&>(swapChain).GetSwapChain();

    auto* dxDescriptorHeap = dynamic_cast<DX12DescriptorHeap&>(*m_rtvDescriptorHeap).GetDescriptorHeap();
    D3D12_CPU_DESCRIPTOR_HANDLE handle = dxDescriptorHeap->GetCPUDescriptorHandleForHeapStart();
    size_t descriptorsCount = m_rtvHandles.size();
    UINT incSize = dxDevice->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
    handle.ptr += descriptorsCount * incSize;

    DXGI_SWAP_CHAIN_DESC swapChainDesc = {};
    dxSwapChain->GetDesc(&swapChainDesc);

    std::vector<std::shared_ptr<IRHIRenderTarget>> outVector = {};

    D3D12_CLEAR_VALUE defaultClearValue = {};
    defaultClearValue.Format = swapChainDesc.BufferDesc.Format;
    defaultClearValue.Color[0] = 0.0f;
    defaultClearValue.Color[1] = 0.0f;
    defaultClearValue.Color[2] = 0.0f;
    defaultClearValue.Color[3] = 1.0f;

    for (UINT i = 0; i < swapChainDesc.BufferCount; ++i)
    {
        ID3D12Resource* resource = nullptr;
        dxSwapChain->GetBuffer(i, IID_PPV_ARGS(&resource));
        D3D12_RENDER_TARGET_VIEW_DESC viewDesc = {};
        viewDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
        viewDesc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2D;
        dxDevice->CreateRenderTargetView(resource, &viewDesc, handle);
        resource->SetName(L"SwapChain RT");

        std::shared_ptr<IRHIRenderTarget> newRenderTarget = RHIResourceFactory::CreateRHIResource<IRHIRenderTarget>();
        newRenderTarget->SetRenderTargetType(RHIRenderTargetType::RTV);
        newRenderTarget->SetRenderTargetFormat(RHIDataFormat::R8G8B8A8_UNORM_SRGB);
        DX12RenderTarget* dxRenderTarget = dynamic_cast<DX12RenderTarget*>(newRenderTarget.get());
        dxRenderTarget->SetRenderTarget(resource, false);
        dxRenderTarget->SetClearValue(defaultClearValue);
        outVector.push_back(newRenderTarget);
        
        m_rtvHandles[newRenderTarget->GetRenderTargetId()] = handle;
        handle.ptr += incSize;
    }

    return outVector;
}

bool DX12RenderTargetManager::ClearRenderTarget(IRHICommandList& commandList, IRHIRenderTarget* renderTargetArray,
    size_t renderTargetArrayCount)
{
    auto* dxCommandList = dynamic_cast<DX12CommandList&>(commandList).GetCommandList();
    for (size_t i = 0; i < renderTargetArrayCount; ++i)
    {
        auto* dxRenderTarget = dynamic_cast<DX12RenderTarget*>(&renderTargetArray[i]);
        switch (dxRenderTarget->GetRenderTargetType())
        {
        case RHIRenderTargetType::RTV:
            {
                assert(m_rtvHandles.find(dxRenderTarget->GetRenderTargetId()) != m_rtvHandles.end());
                const auto& rtvHandle = m_rtvHandles[dxRenderTarget->GetRenderTargetId()];
                dxCommandList->ClearRenderTargetView(rtvHandle, dxRenderTarget->GetClearValue().Color, 0, nullptr);    
            }
            break;

        case RHIRenderTargetType::DSV:
            {
                assert(m_dsvHandles.find(dxRenderTarget->GetRenderTargetId()) != m_dsvHandles.end());
                const auto& dsvHandle = m_dsvHandles[dxRenderTarget->GetRenderTargetId()];
                dxCommandList->ClearDepthStencilView(dsvHandle, D3D12_CLEAR_FLAG_DEPTH | D3D12_CLEAR_FLAG_STENCIL,
                    dxRenderTarget->GetClearValue().DepthStencil.Depth, dxRenderTarget->GetClearValue().DepthStencil.Stencil, 0, nullptr);
            }
            break;
            
        default:
            assert(false);
        }
    }

    return true;
}

bool DX12RenderTargetManager::BindRenderTarget(IRHICommandList& commandList, IRHIRenderTarget* renderTargetArray,
    size_t renderTargetArraySize, IRHIRenderTarget* depthStencil)
{
    auto* dxCommandList = dynamic_cast<DX12CommandList&>(commandList).GetCommandList();
    if (!renderTargetArraySize)
    {
        // Bind zero rt? some bugs must exists.. 
        assert(false);
        return false;
    }
    
    std::vector<D3D12_CPU_DESCRIPTOR_HANDLE> renderTargetViews(renderTargetArraySize);
    for (size_t i = 0; i < renderTargetArraySize; ++i)
    {
        auto iter = m_rtvHandles.find(renderTargetArray[i].GetRenderTargetId());
        assert(iter != m_rtvHandles.end());
        renderTargetViews[i] = iter->second;
    }

    D3D12_CPU_DESCRIPTOR_HANDLE* dsHandle = nullptr;
    if (depthStencil)
    {
        auto iter = m_dsvHandles.find(depthStencil->GetRenderTargetId());
        assert(iter != m_dsvHandles.end());
        dsHandle = &iter->second;
    }

    // TODO: Check RTsSingleHandleToDescriptorRange means?
    dxCommandList->OMSetRenderTargets(renderTargetViews.size(), renderTargetViews.data(), false, dsHandle);
    
    return true;
}