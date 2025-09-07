#include "DX12DescriptorHeap.h"

#include "d3dx12.h"
#include "DX12ConverterUtils.h"
#include "DX12Device.h"
#include "DX12Buffer.h"
#include "DX12DescriptorManager.h"
#include "DX12Texture.h"
#include "DX12Utils.h"
#include "RHIResourceFactoryImpl.hpp"

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
                                                                  std::shared_ptr<IRHIBuffer> buffer, const RHIConstantBufferViewDesc& desc, std::shared_ptr<IRHIDescriptorAllocation>& out_allocation)
{
    //TODO: Process offset for handle 
    auto* dxDevice = dynamic_cast<DX12Device&>(device).GetDevice();
    auto* dxBuffer = dynamic_cast<DX12Buffer&>(*buffer).GetRawBuffer();
    
    CD3DX12_CPU_DESCRIPTOR_HANDLE cpuHandle(m_descriptorHeap->GetCPUDescriptorHandleForHeapStart());
    cpuHandle.Offset(descriptor_offset, m_descriptor_increment_size);
    
    D3D12_CONSTANT_BUFFER_VIEW_DESC cbvDesc = {};
    cbvDesc.BufferLocation = dxBuffer->GetGPUVirtualAddress();
    cbvDesc.SizeInBytes = (desc.bufferSize + 255) & ~255;    // CB size is required to be 256-byte aligned.
    dxDevice->CreateConstantBufferView(&cbvDesc, cpuHandle);

    CD3DX12_GPU_DESCRIPTOR_HANDLE gpuHandle(m_descriptorHeap->GetGPUDescriptorHandleForHeapStart());
    gpuHandle.Offset(descriptor_offset, m_descriptor_increment_size);

    out_allocation = RHIResourceFactory::CreateRHIResource<IRHIBufferDescriptorAllocation>();
    dynamic_cast<IRHIBufferDescriptorAllocation&>(*out_allocation).InitFromBuffer(buffer, RHIBufferDescriptorDesc
        {
            RHIDataFormat::UNKNOWN,
            RHIViewType::RVT_CBV,
            static_cast<unsigned>(desc.bufferSize),
            0
        });
    
    dynamic_cast<DX12BufferDescriptorAllocation&>(*out_allocation).InitHandle(gpuHandle.ptr, 0);

    ++m_used_descriptor_count;
    
    return true;
}

bool DX12DescriptorHeap::CreateResourceDescriptorInHeap(IRHIDevice& device,
                                                                  const std::shared_ptr<IRHIBuffer>& buffer, const RHIDescriptorDesc& desc, std::shared_ptr<IRHIBufferDescriptorAllocation>& out_allocation)
{
    auto* resource = dynamic_cast<const DX12Buffer&>(*buffer).GetRawBuffer();
    auto find_resource = m_created_descriptors_info.find(resource);
    auto& buffer_desc = dynamic_cast<const RHIBufferDescriptorDesc&>(desc);
    
    if (find_resource != m_created_descriptors_info.end())
    {
        for (const auto& created_info : find_resource->second)
        {
            if (created_info.first == desc)
            {
                out_allocation = RHIResourceFactory::CreateRHIResource<IRHIBufferDescriptorAllocation>();
                out_allocation->InitFromBuffer(buffer, buffer_desc);
    
                dynamic_cast<DX12BufferDescriptorAllocation&>(*out_allocation).InitHandle(created_info.second.second, created_info.second.first);
                return true;
            }
        }
    }
    
    bool created = false;
    RHIGPUDescriptorHandle gpu_handle {0};
    RHICPUDescriptorHandle cpu_handle {0};
    
    switch (desc.m_view_type)
    {
    case RHIViewType::RVT_CBV:
        break;
    case RHIViewType::RVT_SRV:    
        created = CreateSRVInHeap(device, m_used_descriptor_count, resource, desc, cpu_handle, gpu_handle);
        break;
    case RHIViewType::RVT_UAV:
        created = CreateUAVInHeap(device, m_used_descriptor_count, resource, desc, cpu_handle, gpu_handle);
        break;
    case RHIViewType::RVT_RTV:
        created = CreateRTVInHeap(device, m_used_descriptor_count, resource, desc, cpu_handle);
        break;
    case RHIViewType::RVT_DSV:
        created = CreateDSVInHeap(device, m_used_descriptor_count, resource, desc, cpu_handle);
        break;
    }

    out_allocation = RHIResourceFactory::CreateRHIResource<IRHIBufferDescriptorAllocation>();
    out_allocation->InitFromBuffer(buffer, buffer_desc);
    dynamic_cast<DX12BufferDescriptorAllocation&>(*out_allocation).InitHandle(gpu_handle, cpu_handle);
    
    return created;
}

bool DX12DescriptorHeap::CreateResourceDescriptorInHeap(IRHIDevice& device, const std::shared_ptr<IRHITexture>& texture,
                                                                  const RHIDescriptorDesc& desc, std::shared_ptr<IRHITextureDescriptorAllocation>& out_allocation)
{
    auto* resource = dynamic_cast<DX12Texture&>(*texture).GetRawResource();
    auto find_resource = m_created_descriptors_info.find(resource);
    auto& texture_desc = dynamic_cast<const RHITextureDescriptorDesc&>(desc);
    if (find_resource != m_created_descriptors_info.end())
    {
        for (const auto& created_info : find_resource->second)
        {
            if (created_info.first == desc)
            {
                out_allocation = std::make_shared<DX12TextureDescriptorAllocation>(created_info.second.second, created_info.second.first, texture, texture_desc);
                return true;
            }
        }
    }
    
    bool created = false;
    RHIGPUDescriptorHandle gpu_handle {0};
    RHICPUDescriptorHandle cpu_handle {0};
    
    switch (desc.m_view_type)
    {
    case RHIViewType::RVT_CBV:
        break;
    case RHIViewType::RVT_SRV:    
        created = CreateSRVInHeap(device, m_used_descriptor_count, resource, desc, cpu_handle, gpu_handle);
        break;
    case RHIViewType::RVT_UAV:
        created = CreateUAVInHeap(device, m_used_descriptor_count, resource, desc, cpu_handle, gpu_handle);
        break;
    case RHIViewType::RVT_RTV:
        created = CreateRTVInHeap(device, m_used_descriptor_count, resource, desc, cpu_handle);
        break;
    case RHIViewType::RVT_DSV:
        created = CreateDSVInHeap(device, m_used_descriptor_count, resource, desc, cpu_handle);
        break;
    }

    out_allocation = std::make_shared<DX12TextureDescriptorAllocation>(gpu_handle, cpu_handle, texture, texture_desc);
    return created;
}

bool DX12DescriptorHeap::Release(IRHIMemoryManager& memory_manager)
{
    SAFE_RELEASE(m_descriptorHeap)
    
    return true;
}

D3D12_CPU_DESCRIPTOR_HANDLE DX12DescriptorHeap::GetCPUHandleForHeapStart() const
{
    return m_descriptorHeap->GetCPUDescriptorHandleForHeapStart();
}

D3D12_GPU_DESCRIPTOR_HANDLE DX12DescriptorHeap::GetGPUHandleForHeapStart() const
{
    return m_descriptorHeap->GetGPUDescriptorHandleForHeapStart();
}

D3D12_CPU_DESCRIPTOR_HANDLE DX12DescriptorHeap::GetAvailableCPUHandle() const
{
    CD3DX12_CPU_DESCRIPTOR_HANDLE cpu_handle(GetCPUHandleForHeapStart());
    cpu_handle.Offset(GetUsedDescriptorCount(), m_descriptor_increment_size);
    return cpu_handle;
}

D3D12_GPU_DESCRIPTOR_HANDLE DX12DescriptorHeap::GetAvailableGPUHandle() const
{
    CD3DX12_GPU_DESCRIPTOR_HANDLE gpu_handle(GetGPUHandleForHeapStart());
    gpu_handle.Offset(GetUsedDescriptorCount(), m_descriptor_increment_size);
    return gpu_handle;
}

bool DX12DescriptorHeap::CreateSRVInHeap(IRHIDevice& device, unsigned descriptor_offset,
                                         ID3D12Resource* resource, const RHIDescriptorDesc& desc, RHICPUDescriptorHandle& out_CPU_handle, RHIGPUDescriptorHandle& out_GPU_handle)
{
    GLTF_CHECK(m_desc.type == RHIDescriptorHeapType::CBV_SRV_UAV_GPU);
    
    //TODO: Process offset for handle 
    auto* dxDevice = dynamic_cast<DX12Device&>(device).GetDevice();
    
    D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
    srvDesc.Format = DX12ConverterUtils::ConvertToDXGIFormat(desc.m_format);
    srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
    srvDesc.ViewDimension = DX12ConverterUtils::ConvertToSRVDimensionType(desc.m_dimension);

    if (desc.m_dimension == RHIResourceDimension::TEXTURE2D)
    {
        srvDesc.Texture2D.MostDetailedMip = 0;
        srvDesc.Texture2D.MipLevels = -1;  
    }
    else if (desc.m_dimension == RHIResourceDimension::BUFFER)
    {
        const RHIBufferDescriptorDesc& buffer_srv_desc = dynamic_cast<const RHIBufferDescriptorDesc&>(desc);
        srvDesc.Buffer.StructureByteStride = buffer_srv_desc.m_uav_structured_buffer_desc.is_structured_buffer ? buffer_srv_desc.m_uav_structured_buffer_desc.stride : 0;
        srvDesc.Buffer.FirstElement = 0;
        srvDesc.Buffer.NumElements = buffer_srv_desc.m_uav_structured_buffer_desc.count;
        srvDesc.Buffer.Flags = D3D12_BUFFER_SRV_FLAG_NONE;
    }

    CD3DX12_CPU_DESCRIPTOR_HANDLE cpuHandle(m_descriptorHeap->GetCPUDescriptorHandleForHeapStart());
    cpuHandle.Offset(descriptor_offset, m_descriptor_increment_size);
    out_CPU_handle = cpuHandle.ptr;
    
    dxDevice->CreateShaderResourceView(resource, &srvDesc, cpuHandle);
    CD3DX12_GPU_DESCRIPTOR_HANDLE gpuHandle(m_descriptorHeap->GetGPUDescriptorHandleForHeapStart());
    gpuHandle.Offset(descriptor_offset, m_descriptor_increment_size);
    out_GPU_handle = gpuHandle.ptr;
    
    ++m_used_descriptor_count;
    m_created_descriptors_info[resource].emplace_back(desc, std::pair{out_CPU_handle, out_GPU_handle} );
    
    return true;
}

bool DX12DescriptorHeap::CreateUAVInHeap(IRHIDevice& device, unsigned descriptor_offset, ID3D12Resource* resource,
                                         const RHIDescriptorDesc& desc, RHICPUDescriptorHandle& out_CPU_handle, RHIGPUDescriptorHandle& out_GPU_handle)
{
    GLTF_CHECK(m_desc.type == RHIDescriptorHeapType::CBV_SRV_UAV_GPU);
    //TODO: Process offset for handle 
    auto* dxDevice = dynamic_cast<DX12Device&>(device).GetDevice();
    
    D3D12_UNORDERED_ACCESS_VIEW_DESC UAVDesc = {};
    UAVDesc.Format = DX12ConverterUtils::ConvertToDXGIFormat(desc.m_format);
    UAVDesc.ViewDimension = DX12ConverterUtils::ConvertToUAVDimensionType(desc.m_dimension);

    if (desc.m_dimension == RHIResourceDimension::TEXTURE2D)
    {
        UAVDesc.Texture2D.MipSlice = 0;
        UAVDesc.Texture2D.PlaneSlice = 0;    
    }
    else if (desc.m_dimension == RHIResourceDimension::BUFFER)
    {
        const RHIBufferDescriptorDesc& buffer_uav_desc = dynamic_cast<const RHIBufferDescriptorDesc&>(desc);
        if (buffer_uav_desc.m_uav_structured_buffer_desc.use_count_buffer)
        {
            UAVDesc.Buffer.CounterOffsetInBytes = buffer_uav_desc.m_uav_structured_buffer_desc.count_buffer_offset;
        }
        UAVDesc.Buffer.StructureByteStride = buffer_uav_desc.m_uav_structured_buffer_desc.is_structured_buffer ? buffer_uav_desc.m_uav_structured_buffer_desc.stride : 0;
        UAVDesc.Buffer.FirstElement = 0;
        UAVDesc.Buffer.NumElements = buffer_uav_desc.m_uav_structured_buffer_desc.count;
        UAVDesc.Buffer.Flags = D3D12_BUFFER_UAV_FLAG_NONE;
    }

    CD3DX12_CPU_DESCRIPTOR_HANDLE cpuHandle(m_descriptorHeap->GetCPUDescriptorHandleForHeapStart());
    cpuHandle.Offset(static_cast<int>(descriptor_offset), m_descriptor_increment_size);
    out_CPU_handle = cpuHandle.ptr;
    
    dxDevice->CreateUnorderedAccessView(resource, UAVDesc.Buffer.CounterOffsetInBytes > 0 ? resource : nullptr, &UAVDesc, cpuHandle);
    CD3DX12_GPU_DESCRIPTOR_HANDLE gpuHandle(m_descriptorHeap->GetGPUDescriptorHandleForHeapStart());
    gpuHandle.Offset(static_cast<int>(descriptor_offset), m_descriptor_increment_size);
    out_GPU_handle = gpuHandle.ptr;
    
    ++m_used_descriptor_count;
    m_created_descriptors_info[resource].emplace_back(desc, std::pair{out_CPU_handle, out_GPU_handle});
    
    return true;
}

bool DX12DescriptorHeap::CreateRTVInHeap(IRHIDevice& device, unsigned descriptor_offset, ID3D12Resource* resource,
    const RHIDescriptorDesc& desc, RHICPUDescriptorHandle& out_CPU_handle)
{
    GLTF_CHECK(m_desc.type == RHIDescriptorHeapType::RTV);
    auto* dx_device = dynamic_cast<DX12Device&>(device).GetDevice();
    auto* dx_descriptor_heap = GetDescriptorHeap();
    
    CD3DX12_CPU_DESCRIPTOR_HANDLE cpu_handle = CD3DX12_CPU_DESCRIPTOR_HANDLE(dx_descriptor_heap->GetCPUDescriptorHandleForHeapStart());
    cpu_handle.Offset(static_cast<int>(descriptor_offset), m_descriptor_increment_size);
    
    D3D12_RENDER_TARGET_VIEW_DESC rtv_desc = {};
    rtv_desc.Format = DX12ConverterUtils::ConvertToDXGIFormat(desc.m_format);
    rtv_desc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2D;
    dx_device->CreateRenderTargetView(resource, &rtv_desc, cpu_handle);

    out_CPU_handle = cpu_handle.ptr;
    
    ++m_used_descriptor_count;
    m_created_descriptors_info[resource].emplace_back(desc, std::pair{out_CPU_handle, 0});

    return true;
}

bool DX12DescriptorHeap::CreateDSVInHeap(IRHIDevice& device, unsigned descriptor_offset, ID3D12Resource* resource,
    const RHIDescriptorDesc& desc, RHICPUDescriptorHandle& out_CPU_handle)
{
    GLTF_CHECK(m_desc.type == RHIDescriptorHeapType::DSV);
    auto* dx_device = dynamic_cast<DX12Device&>(device).GetDevice();
    auto* dx_descriptor_heap = GetDescriptorHeap();
    
    CD3DX12_CPU_DESCRIPTOR_HANDLE cpu_handle = CD3DX12_CPU_DESCRIPTOR_HANDLE(dx_descriptor_heap->GetCPUDescriptorHandleForHeapStart());
    cpu_handle.Offset(static_cast<int>(descriptor_offset), m_descriptor_increment_size);
    
    D3D12_DEPTH_STENCIL_VIEW_DESC dsv_desc = {};
    dsv_desc.Format = DX12ConverterUtils::ConvertToDXGIFormat(desc.m_format);
    dsv_desc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2D;
    dx_device->CreateDepthStencilView(resource, &dsv_desc, cpu_handle);

    out_CPU_handle = cpu_handle.ptr;
    
    ++m_used_descriptor_count;
    m_created_descriptors_info[resource].emplace_back(desc, std::pair{out_CPU_handle, 0});

    return true;
}
