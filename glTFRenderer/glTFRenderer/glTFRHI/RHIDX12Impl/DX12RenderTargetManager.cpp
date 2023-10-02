

#include "DX12RenderTargetManager.h"
#include "DX12CommandList.h"
#include "DX12DescriptorHeap.h"
#include "DX12Utils.h"
#include "glTFRHI/RHIResourceFactoryImpl.hpp"

UINT GetDescriptorIncSize(ID3D12Device* device, RHIRenderTargetType type)
{
    UINT result = 0;
    switch (type) {
    case RHIRenderTargetType::RTV:
        result = device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
        break;
    case RHIRenderTargetType::DSV:
        result = device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_DSV);
        break;
    case RHIRenderTargetType::Unknown:
        break;
    default: GLTF_CHECK(false);
    }
    return result;
}

DX12RenderTargetManager::DX12RenderTargetManager()
    : m_max_render_target_count(0)
    , m_rtv_descriptor_heap(nullptr)
    , m_dsv_descriptor_heap(nullptr)
{
}

DX12RenderTargetManager::~DX12RenderTargetManager()
= default;

bool DX12RenderTargetManager::InitRenderTargetManager(IRHIDevice& device, size_t max_render_target_count)
{
    m_rtv_descriptor_heap = RHIResourceFactory::CreateRHIResource<IRHIDescriptorHeap>();
    m_rtv_descriptor_heap->InitDescriptorHeap(device, {static_cast<unsigned>(max_render_target_count), RHIDescriptorHeapType::RTV, false});

    m_dsv_descriptor_heap = RHIResourceFactory::CreateRHIResource<IRHIDescriptorHeap>();
    m_dsv_descriptor_heap->InitDescriptorHeap(device, {static_cast<unsigned>(max_render_target_count), RHIDescriptorHeapType::DSV, false});
    
    m_max_render_target_count = max_render_target_count;
    
    return true;
}

std::shared_ptr<IRHIRenderTarget> DX12RenderTargetManager::CreateRenderTarget(IRHIDevice& device, RHIRenderTargetType type,
    RHIDataFormat resource_format, RHIDataFormat descriptor_format, const IRHIRenderTargetDesc& desc)
{
    auto* dxDevice = dynamic_cast<DX12Device&>(device).GetDevice();
    
    // Check has enough space for descriptor?
    D3D12_RESOURCE_FLAGS flags = (desc.isUAV) ? D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS : D3D12_RESOURCE_FLAG_NONE;
    D3D12_RESOURCE_STATES state = D3D12_RESOURCE_STATE_COMMON;
    const DXGI_FORMAT dxResourceFormat = DX12ConverterUtils::ConvertToDXGIFormat(resource_format);
    
    switch (type)
    {
    case RHIRenderTargetType::RTV:
        {
            flags |= D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET;
            state = D3D12_RESOURCE_STATE_RENDER_TARGET;
        }
        break;
    case RHIRenderTargetType::DSV:
        {
            flags |= D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL;
            state = D3D12_RESOURCE_STATE_DEPTH_WRITE;
        }
        break;
    }
    
    D3D12_HEAP_PROPERTIES heap_properties = {};
    heap_properties.Type = D3D12_HEAP_TYPE_DEFAULT;
    heap_properties.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;

    D3D12_RESOURCE_DESC resource_desc = {};
    resource_desc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
    resource_desc.Format = dxResourceFormat;
    resource_desc.Width = desc.width;
    resource_desc.Height = desc.height;
    resource_desc.DepthOrArraySize = 1;
    resource_desc.MipLevels = 1;
    resource_desc.Alignment = 0;
    resource_desc.SampleDesc.Count = 1;
    resource_desc.SampleDesc.Quality = 0;
    resource_desc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
    resource_desc.Flags = flags;

    ID3D12Resource* resource = nullptr;
    const auto dx_clear_value = DX12ConverterUtils::ConvertToD3DClearValue(desc.clearValue);
    THROW_IF_FAILED(dxDevice->CreateCommittedResource(&heap_properties,
                                                  D3D12_HEAP_FLAG_NONE,
                                                  &resource_desc,
                                                  state,
                                                  &dx_clear_value,
                                                  IID_PPV_ARGS(&resource)))

    if (!desc.name.empty())
    {
        resource->SetName(to_wide_string(desc.name).c_str());
    }

    return CreateRenderTargetWithResource(device, type, descriptor_format, resource, dx_clear_value);
}

std::vector<std::shared_ptr<IRHIRenderTarget>> DX12RenderTargetManager::CreateRenderTargetFromSwapChain(
    IRHIDevice& device, IRHISwapChain& swapChain, RHIRenderTargetClearValue clearValue)
{
    auto* dxDevice = dynamic_cast<DX12Device&>(device).GetDevice();
    auto* dxSwapChain = dynamic_cast<DX12SwapChain&>(swapChain).GetSwapChain();

    auto* dxDescriptorHeap = dynamic_cast<DX12DescriptorHeap&>(*m_rtv_descriptor_heap).GetDescriptorHeap();
    D3D12_CPU_DESCRIPTOR_HANDLE handle = dxDescriptorHeap->GetCPUDescriptorHandleForHeapStart();
    size_t descriptorsCount = m_rtvHandles.size();
    UINT incSize = dxDevice->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
    handle.ptr += descriptorsCount * incSize;

    DXGI_SWAP_CHAIN_DESC swapChainDesc = {};
    dxSwapChain->GetDesc(&swapChainDesc);

    std::vector<std::shared_ptr<IRHIRenderTarget>> outVector = {};

    D3D12_CLEAR_VALUE dx_clear_value = DX12ConverterUtils::ConvertToD3DClearValue(clearValue);

    for (UINT i = 0; i < swapChainDesc.BufferCount; ++i)
    {
        ID3D12Resource* resource = nullptr;
        dxSwapChain->GetBuffer(i, IID_PPV_ARGS(&resource));
        auto render_target = CreateRenderTargetWithResource(device, RHIRenderTargetType::RTV, RHIDataFormat::R8G8B8A8_UNORM_SRGB, resource, dx_clear_value);
        outVector.push_back(render_target);
    }

    return outVector;
}

bool DX12RenderTargetManager::ClearRenderTarget(IRHICommandList& commandList, const std::vector<IRHIRenderTarget*>& renderTargets)
{
    auto* dxCommandList = dynamic_cast<DX12CommandList&>(commandList).GetCommandList();
    for (size_t i = 0; i < renderTargets.size(); ++i)
    {
        auto* dxRenderTarget = dynamic_cast<DX12RenderTarget*>(renderTargets[i]);
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

bool DX12RenderTargetManager::BindRenderTarget(IRHICommandList& commandList, const std::vector<IRHIRenderTarget*>& renderTargets,
                                               IRHIRenderTarget* depthStencil)
{
    auto* dxCommandList = dynamic_cast<DX12CommandList&>(commandList).GetCommandList();
    if (renderTargets.empty() && !depthStencil)
    {
        // Bind zero rt? some bugs must exists.. 
        assert(false);
        return false;
    }
    
    std::vector<D3D12_CPU_DESCRIPTOR_HANDLE> renderTargetViews(renderTargets.size());
    for (size_t i = 0; i < renderTargetViews.size(); ++i)
    {
        auto iter = m_rtvHandles.find(renderTargets[i]->GetRenderTargetId());
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

std::shared_ptr<IRHIRenderTarget> DX12RenderTargetManager::CreateRenderTargetWithResource(IRHIDevice& device, RHIRenderTargetType type,
    RHIDataFormat descriptor_format, ID3D12Resource* texture_resource, const D3D12_CLEAR_VALUE& clear_value)
{
    auto* dx_device = dynamic_cast<DX12Device&>(device).GetDevice();
    auto& heap = (type == RHIRenderTargetType::RTV) ? *m_rtv_descriptor_heap : *m_dsv_descriptor_heap; 
    auto& heap_descriptor_array = (type == RHIRenderTargetType::RTV) ? m_rtvHandles : m_dsvHandles;
    
    auto* dxDescriptorHeap = dynamic_cast<DX12DescriptorHeap&>(heap).GetDescriptorHeap();
    
    D3D12_CPU_DESCRIPTOR_HANDLE handle = dxDescriptorHeap->GetCPUDescriptorHandleForHeapStart();
    const size_t descriptors_count = heap_descriptor_array.size();
    const UINT descriptor_inc_size = GetDescriptorIncSize(dx_device, type);
    handle.ptr += descriptors_count * descriptor_inc_size;
    const auto dx_descriptor_format = DX12ConverterUtils::ConvertToDXGIFormat(descriptor_format);
    
    switch (type) {
    case RHIRenderTargetType::RTV:
        {
            D3D12_RENDER_TARGET_VIEW_DESC rtv_desc = {};
            rtv_desc.Format = dx_descriptor_format;
            rtv_desc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2D;
            dx_device->CreateRenderTargetView(texture_resource, &rtv_desc, handle);    
        }
        break;
    case RHIRenderTargetType::DSV:
        {
            D3D12_DEPTH_STENCIL_VIEW_DESC dsv_desc = {};
            dsv_desc.Format = dx_descriptor_format;
            dsv_desc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2D;
            dx_device->CreateDepthStencilView(texture_resource, &dsv_desc, handle);
        }
        break;
    default:
        GLTF_CHECK(false);
    }
    
    std::shared_ptr<IRHIRenderTarget> render_target = RHIResourceFactory::CreateRHIResource<IRHIRenderTarget>();
    render_target->SetRenderTargetType(type);
    render_target->SetRenderTargetFormat(descriptor_format);
    
    DX12RenderTarget* dxRenderTarget = dynamic_cast<DX12RenderTarget*>(render_target.get());
    dxRenderTarget->SetRenderTarget(texture_resource);
    dxRenderTarget->SetClearValue(clear_value);
    
    heap_descriptor_array[render_target->GetRenderTargetId()] = handle;
    
    return render_target;
}
