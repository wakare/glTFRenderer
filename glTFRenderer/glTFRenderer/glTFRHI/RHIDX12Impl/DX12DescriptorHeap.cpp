#include "DX12DescriptorHeap.h"

#include "d3dx12.h"
#include "DX12ConverterUtils.h"
#include "DX12Device.h"
#include "DX12Buffer.h"
#include "DX12RenderTarget.h"
#include "DX12Texture.h"
#include "DX12Utils.h"

bool DX12DescriptorAllocation::InitFromBuffer(const IRHIBuffer& buffer)
{
    m_gpu_handle = dynamic_cast<const DX12Buffer&>(buffer).GetBuffer()->GetGPUVirtualAddress();
    return true;
}

bool DX12DescriptorTable::Build(IRHIDevice& device, const std::vector<std::shared_ptr<IRHIDescriptorAllocation>>& descriptor_allocations)
{
    GLTF_CHECK (!descriptor_allocations.empty());
    
    auto* dxDevice = dynamic_cast<DX12Device&>(device).GetDevice();
    auto descriptor_increment_size = dxDevice->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
    
    // Check all allocation gpu handle is consistent!!
    bool is_consistent_gpu_handle = true;

    CD3DX12_GPU_DESCRIPTOR_HANDLE current_handle ({dynamic_cast<const DX12DescriptorAllocation&>(*descriptor_allocations[0]).m_gpu_handle});
    m_gpu_handle = current_handle.ptr;
    
    for (size_t i = 1; i < descriptor_allocations.size(); ++i)
    {
        auto check_handle = current_handle.Offset(1, descriptor_increment_size).ptr;
        GLTF_CHECK(dynamic_cast<const DX12DescriptorAllocation&>(*descriptor_allocations[i]).m_gpu_handle == check_handle);
    }

    return true; 
}

DX12DescriptorHeap::DX12DescriptorHeap()
    : m_descriptorHeap(nullptr)
    , m_descriptor_increment_size(0)
    , m_used_descriptor_count(0)
{
}

DX12DescriptorHeap::~DX12DescriptorHeap()
{
    SAFE_RELEASE(m_descriptorHeap)
}

bool DX12DescriptorHeap::InitDescriptorHeap(IRHIDevice& device, const RHIDescriptorHeapDesc& desc)
{
    m_desc = desc;
    auto* dxDevice = dynamic_cast<DX12Device&>(device).GetDevice();
    
    // Init resource description heap
    D3D12_DESCRIPTOR_HEAP_DESC dxDesc = {};
    dxDesc.NumDescriptors = desc.max_descriptor_count;
    dxDesc.Type = DX12ConverterUtils::ConvertToDescriptorHeapType(desc.type);
    dxDesc.Flags = desc.shader_visible ? D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE : D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
    dxDesc.NodeMask = 0;
    
    THROW_IF_FAILED(dxDevice->CreateDescriptorHeap(&dxDesc, IID_PPV_ARGS(&m_descriptorHeap)))
    m_descriptor_increment_size = dxDevice->GetDescriptorHandleIncrementSize(dxDesc.Type);
    
    return true;
}

unsigned DX12DescriptorHeap::GetUsedDescriptorCount() const
{
    return m_used_descriptor_count;
}

bool DX12DescriptorHeap::CreateConstantBufferViewInDescriptorHeap(IRHIDevice& device, unsigned descriptor_offset,
                                                                  IRHIBuffer& buffer, const RHIConstantBufferViewDesc& desc, std::shared_ptr<IRHIDescriptorAllocation>& out_allocation)
{
    //TODO: Process offset for handle 
    auto* dxDevice = dynamic_cast<DX12Device&>(device).GetDevice();
    auto* dxBuffer = dynamic_cast<DX12Buffer&>(buffer).GetBuffer();
    
    CD3DX12_CPU_DESCRIPTOR_HANDLE cpuHandle(m_descriptorHeap->GetCPUDescriptorHandleForHeapStart());
    cpuHandle.Offset(descriptor_offset, m_descriptor_increment_size);
    
    D3D12_CONSTANT_BUFFER_VIEW_DESC cbvDesc = {};
    cbvDesc.BufferLocation = dxBuffer->GetGPUVirtualAddress();
    cbvDesc.SizeInBytes = (desc.bufferSize + 255) & ~255;    // CB size is required to be 256-byte aligned.
    dxDevice->CreateConstantBufferView(&cbvDesc, cpuHandle);

    CD3DX12_GPU_DESCRIPTOR_HANDLE gpuHandle(m_descriptorHeap->GetGPUDescriptorHandleForHeapStart());
    gpuHandle.Offset(descriptor_offset, m_descriptor_increment_size);

    out_allocation = std::make_shared<DX12DescriptorAllocation>(gpuHandle.ptr, 0);

    ++m_used_descriptor_count;
    
    return true;
}

bool DX12DescriptorHeap::CreateResourceDescriptorInHeap(IRHIDevice& device,
                                                                  const IRHIBuffer& buffer, const RHIShaderResourceViewDesc& desc, std::shared_ptr<IRHIDescriptorAllocation>& out_allocation)
{
    auto* resource = dynamic_cast<const DX12Buffer&>(buffer).GetBuffer();
    auto find_resource = m_created_descriptors_info.find(resource);
    if (find_resource != m_created_descriptors_info.end())
    {
        for (const auto& created_info : find_resource->second)
        {
            if (created_info.first == desc)
            {
                out_allocation = std::make_shared<DX12DescriptorAllocation>(created_info.second, 0);
                return true;
            }
        }
    }
    
    bool created = false;
    RHIGPUDescriptorHandle gpu_handle {0};
    RHICPUDescriptorHandle cpu_handle {0};
    
    switch (desc.view_type)
    {
    case RHIViewType::RVT_CBV:
        break;
    case RHIViewType::RVT_SRV:    
        created = CreateSRVInHeap(device, m_used_descriptor_count, resource, desc, gpu_handle);
        break;
    case RHIViewType::RVT_UAV:
        created = CreateUAVInHeap(device, m_used_descriptor_count, resource, desc, gpu_handle);
        break;
    case RHIViewType::RVT_RTV:
        created = CreateRTVInHeap(device, m_used_descriptor_count, resource, desc, cpu_handle);
        break;
    case RHIViewType::RVT_DSV:
        created = CreateDSVInHeap(device, m_used_descriptor_count, resource, desc, cpu_handle);
        break;
    }

    out_allocation = std::make_shared<DX12DescriptorAllocation>(gpu_handle, cpu_handle);
    out_allocation->m_view_desc = desc;
    return created;
}

bool DX12DescriptorHeap::CreateResourceDescriptorInHeap(IRHIDevice& device, const IRHITexture& texture,
                                                                  const RHIShaderResourceViewDesc& desc, std::shared_ptr<IRHIDescriptorAllocation>& out_allocation)
{
    auto* resource = dynamic_cast<const DX12Texture&>(texture).GetRawResource();
    auto find_resource = m_created_descriptors_info.find(resource);
    if (find_resource != m_created_descriptors_info.end())
    {
        for (const auto& created_info : find_resource->second)
        {
            if (created_info.first == desc)
            {
                out_allocation = std::make_shared<DX12DescriptorAllocation>(created_info.second, 0);
                return true;
            }
        }
    }
    
    bool created = false;
    RHIGPUDescriptorHandle gpu_handle {0};
    RHICPUDescriptorHandle cpu_handle {0};
    
    switch (desc.view_type)
    {
    case RHIViewType::RVT_CBV:
        break;
    case RHIViewType::RVT_SRV:    
        created = CreateSRVInHeap(device, m_used_descriptor_count, resource, desc, gpu_handle);
        break;
    case RHIViewType::RVT_UAV:
        created = CreateUAVInHeap(device, m_used_descriptor_count, resource, desc, gpu_handle);
        break;
    case RHIViewType::RVT_RTV:
        created = CreateRTVInHeap(device, m_used_descriptor_count, resource, desc, cpu_handle);
        break;
    case RHIViewType::RVT_DSV:
        created = CreateDSVInHeap(device, m_used_descriptor_count, resource, desc, cpu_handle);
        break;
    }

    out_allocation = std::make_shared<DX12DescriptorAllocation>(gpu_handle, cpu_handle);
    out_allocation->m_view_desc = desc;
    return created;
}

bool DX12DescriptorHeap::CreateResourceDescriptorInHeap(IRHIDevice& device,
                                                                  const IRHIRenderTarget& render_target, const RHIShaderResourceViewDesc& desc, std::shared_ptr<IRHIDescriptorAllocation>&
                                                                  out_allocation)
{
    auto* resource = dynamic_cast<const DX12Texture&>(render_target.GetTexture()).GetRawResource();
    
    auto find_resource = m_created_descriptors_info.find(resource);
    if (find_resource != m_created_descriptors_info.end())
    {
        for (const auto& created_info : find_resource->second)
        {
            if (created_info.first == desc)
            {
                out_allocation = std::make_shared<DX12DescriptorAllocation>(created_info.second, 0);
                return true;
            }
        }
    }
    
    bool created = false;
    RHIGPUDescriptorHandle gpu_handle {0};
    RHICPUDescriptorHandle cpu_handle {0};
    
    switch (desc.view_type)
    {
    case RHIViewType::RVT_CBV:
        break;
    case RHIViewType::RVT_SRV:    
        created = CreateSRVInHeap(device, m_used_descriptor_count, resource, desc, gpu_handle);
        break;
    case RHIViewType::RVT_UAV:
        created = CreateUAVInHeap(device, m_used_descriptor_count, resource, desc, gpu_handle);
        break;
    case RHIViewType::RVT_RTV:
        created = CreateRTVInHeap(device, m_used_descriptor_count, resource, desc, cpu_handle);
        break;
    case RHIViewType::RVT_DSV:
        created = CreateDSVInHeap(device, m_used_descriptor_count, resource, desc, cpu_handle);
        break;
    }

    out_allocation = std::make_shared<DX12DescriptorAllocation>(gpu_handle, cpu_handle);
    out_allocation->m_view_desc = desc;
    return created;
}

D3D12_CPU_DESCRIPTOR_HANDLE DX12DescriptorHeap::GetCPUHandleForHeapStart() const
{
    return m_descriptorHeap->GetCPUDescriptorHandleForHeapStart();
}

D3D12_GPU_DESCRIPTOR_HANDLE DX12DescriptorHeap::GetGPUHandleForHeapStart() const
{
    return m_descriptorHeap->GetGPUDescriptorHandleForHeapStart();
}

bool DX12DescriptorHeap::CreateSRVInHeap(IRHIDevice& device, unsigned descriptor_offset,
                                         ID3D12Resource* resource, const RHIShaderResourceViewDesc& desc, RHIGPUDescriptorHandle& out_GPU_handle)
{
    GLTF_CHECK(m_desc.type == RHIDescriptorHeapType::CBV_SRV_UAV);
    
    //TODO: Process offset for handle 
    auto* dxDevice = dynamic_cast<DX12Device&>(device).GetDevice();
    
    D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
    srvDesc.Format = DX12ConverterUtils::ConvertToDXGIFormat(desc.format);
    srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
    srvDesc.ViewDimension = DX12ConverterUtils::ConvertToSRVDimensionType(desc.dimension);

    // TODO: Config mip levels
    srvDesc.Texture2D.MipLevels = 1;

    CD3DX12_CPU_DESCRIPTOR_HANDLE cpuHandle(m_descriptorHeap->GetCPUDescriptorHandleForHeapStart());
    cpuHandle.Offset(descriptor_offset, m_descriptor_increment_size);
    
    dxDevice->CreateShaderResourceView(resource, &srvDesc, cpuHandle);
    CD3DX12_GPU_DESCRIPTOR_HANDLE gpuHandle(m_descriptorHeap->GetGPUDescriptorHandleForHeapStart());
    gpuHandle.Offset(descriptor_offset, m_descriptor_increment_size);
    out_GPU_handle = gpuHandle.ptr;
    
    ++m_used_descriptor_count;
    m_created_descriptors_info[resource].emplace_back(desc, out_GPU_handle);
    
    return true;
}

bool DX12DescriptorHeap::CreateUAVInHeap(IRHIDevice& device, unsigned descriptor_offset, ID3D12Resource* resource,
    const RHIShaderResourceViewDesc& desc, RHIGPUDescriptorHandle& out_GPU_handle)
{
    GLTF_CHECK(m_desc.type == RHIDescriptorHeapType::CBV_SRV_UAV);
    //TODO: Process offset for handle 
    auto* dxDevice = dynamic_cast<DX12Device&>(device).GetDevice();
    
    D3D12_UNORDERED_ACCESS_VIEW_DESC UAVDesc = {};
    UAVDesc.Format = DX12ConverterUtils::ConvertToDXGIFormat(desc.format);
    UAVDesc.ViewDimension = DX12ConverterUtils::ConvertToUAVDimensionType(desc.dimension);

    if (desc.dimension == RHIResourceDimension::TEXTURE2D)
    {
        UAVDesc.Texture2D.MipSlice = 0;
        UAVDesc.Texture2D.PlaneSlice = 0;    
    }
    else if (desc.dimension == RHIResourceDimension::BUFFER)
    {
        if (desc.use_count_buffer)
        {
            UAVDesc.Buffer.CounterOffsetInBytes = desc.count_buffer_offset;
        }
        UAVDesc.Buffer.StructureByteStride = desc.stride;
        UAVDesc.Buffer.FirstElement = 0;
        UAVDesc.Buffer.NumElements = desc.count;
        UAVDesc.Buffer.Flags = D3D12_BUFFER_UAV_FLAG_NONE;
    }

    CD3DX12_CPU_DESCRIPTOR_HANDLE cpuHandle(m_descriptorHeap->GetCPUDescriptorHandleForHeapStart());
    cpuHandle.Offset(static_cast<int>(descriptor_offset), m_descriptor_increment_size);
    
    dxDevice->CreateUnorderedAccessView(resource, desc.use_count_buffer ? resource : nullptr, &UAVDesc, cpuHandle);
    CD3DX12_GPU_DESCRIPTOR_HANDLE gpuHandle(m_descriptorHeap->GetGPUDescriptorHandleForHeapStart());
    gpuHandle.Offset(static_cast<int>(descriptor_offset), m_descriptor_increment_size);
    out_GPU_handle = gpuHandle.ptr;
    
    ++m_used_descriptor_count;
    m_created_descriptors_info[resource].emplace_back(desc, out_GPU_handle);
    
    return true;
}

bool DX12DescriptorHeap::CreateRTVInHeap(IRHIDevice& device, unsigned descriptor_offset, ID3D12Resource* resource,
    const RHIShaderResourceViewDesc& desc, RHICPUDescriptorHandle& out_CPU_handle)
{
    GLTF_CHECK(m_desc.type == RHIDescriptorHeapType::RTV);
    auto* dx_device = dynamic_cast<DX12Device&>(device).GetDevice();
    auto* dx_descriptor_heap = GetDescriptorHeap();
    
    CD3DX12_CPU_DESCRIPTOR_HANDLE cpu_handle = CD3DX12_CPU_DESCRIPTOR_HANDLE(dx_descriptor_heap->GetCPUDescriptorHandleForHeapStart());
    cpu_handle.Offset(static_cast<int>(descriptor_offset), m_descriptor_increment_size);
    
    D3D12_RENDER_TARGET_VIEW_DESC rtv_desc = {};
    rtv_desc.Format = DX12ConverterUtils::ConvertToDXGIFormat(desc.format);
    rtv_desc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2D;
    dx_device->CreateRenderTargetView(resource, &rtv_desc, cpu_handle);

    out_CPU_handle = cpu_handle.ptr;
    
    ++m_used_descriptor_count;
    m_created_descriptors_info[resource].emplace_back(desc, out_CPU_handle);

    return true;
}

bool DX12DescriptorHeap::CreateDSVInHeap(IRHIDevice& device, unsigned descriptor_offset, ID3D12Resource* resource,
    const RHIShaderResourceViewDesc& desc, RHICPUDescriptorHandle& out_CPU_handle)
{
    GLTF_CHECK(m_desc.type == RHIDescriptorHeapType::DSV);
    auto* dx_device = dynamic_cast<DX12Device&>(device).GetDevice();
    auto* dx_descriptor_heap = GetDescriptorHeap();
    
    CD3DX12_CPU_DESCRIPTOR_HANDLE cpu_handle = CD3DX12_CPU_DESCRIPTOR_HANDLE(dx_descriptor_heap->GetCPUDescriptorHandleForHeapStart());
    cpu_handle.Offset(static_cast<int>(descriptor_offset), m_descriptor_increment_size);
    
    D3D12_DEPTH_STENCIL_VIEW_DESC dsv_desc = {};
    dsv_desc.Format = DX12ConverterUtils::ConvertToDXGIFormat(desc.format);
    dsv_desc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2D;
    dx_device->CreateDepthStencilView(resource, &dsv_desc, cpu_handle);

    out_CPU_handle = cpu_handle.ptr;
    
    ++m_used_descriptor_count;
    m_created_descriptors_info[resource].emplace_back(desc, out_CPU_handle);

    return true;
}
