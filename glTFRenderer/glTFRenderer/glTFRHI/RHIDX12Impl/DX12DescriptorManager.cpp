#include "DX12DescriptorManager.h"

#include "glTFRHI/RHIResourceFactoryImpl.hpp"
#include "DX12DescriptorHeap.h"

bool DX12BufferDescriptorAllocation::InitHandle(RHIGPUDescriptorHandle gpu_handle, RHICPUDescriptorHandle cpu_handle)
{
    m_gpu_handle = gpu_handle;
    m_cpu_handle = cpu_handle;
    return true;
}

bool DX12BufferDescriptorAllocation::InitFromBuffer(const std::shared_ptr<IRHIBuffer>& buffer, const RHIBufferDescriptorDesc& desc)
{
    m_source = buffer;
    m_view_desc = desc;
    return true;
}

bool DX12DescriptorTable::Build(IRHIDevice& device, const std::vector<std::shared_ptr<IRHITextureDescriptorAllocation>>& descriptor_allocations)
{
    if (descriptor_allocations.empty())
    {
        //GLTF_CHECK(false);
        return true;
    }
    
    auto* dxDevice = dynamic_cast<DX12Device&>(device).GetDevice();
    auto descriptor_increment_size = dxDevice->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
    
    CD3DX12_GPU_DESCRIPTOR_HANDLE current_handle ({dynamic_cast<const DX12TextureDescriptorAllocation&>(*descriptor_allocations[0]).m_gpu_handle});
    m_gpu_handle = current_handle.ptr;
    
    for (size_t i = 1; i < descriptor_allocations.size(); ++i)
    {
        auto check_handle = current_handle.Offset(1, descriptor_increment_size).ptr;
        GLTF_CHECK(dynamic_cast<const DX12TextureDescriptorAllocation&>(*descriptor_allocations[i]).m_gpu_handle == check_handle);
    }

    return true; 
}

template<>
    std::shared_ptr<DX12DescriptorHeap> RHIResourceFactory::CreateRHIResource()
{
    return std::make_shared<DX12DescriptorHeap>();
}

bool DX12DescriptorManager::Init(IRHIDevice& device, const DescriptorAllocationInfo& max_descriptor_capacity)
{
    m_CBV_SRV_UAV_gpu_heap = RHIResourceFactory::CreateRHIResource<DX12DescriptorHeap>();
    m_CBV_SRV_UAV_gpu_heap->InitDescriptorHeap(device,
        {
            .max_descriptor_count = max_descriptor_capacity.cbv_srv_uav_size,
            .type = RHIDescriptorHeapType::CBV_SRV_UAV_GPU,
            .shader_visible = true
        });

    /*
    m_CBV_SRV_UAV_cpu_heap = RHIResourceFactory::CreateRHIResource<DX12DescriptorHeap>();
    m_CBV_SRV_UAV_cpu_heap->InitDescriptorHeap(device,
        {
            .max_descriptor_count = max_descriptor_capacity.cbv_srv_uav_size,
            .type = RHIDescriptorHeapType::CBV_SRV_UAV_CPU,
            .shader_visible = false
        });
    */
    
    m_RTV_heap = RHIResourceFactory::CreateRHIResource<DX12DescriptorHeap>();
    m_RTV_heap->InitDescriptorHeap(device,
        {
            .max_descriptor_count = max_descriptor_capacity.rtv_size,
            .type = RHIDescriptorHeapType::RTV,
            .shader_visible = false
        });

    m_DSV_heap = RHIResourceFactory::CreateRHIResource<DX12DescriptorHeap>();
    m_DSV_heap->InitDescriptorHeap(device,
        {
            .max_descriptor_count = max_descriptor_capacity.dsv_size,
            .type = RHIDescriptorHeapType::DSV,
            .shader_visible = false
        });

    m_ImGUI_Heap = RHIResourceFactory::CreateRHIResource<DX12DescriptorHeap>();
    m_ImGUI_Heap->InitDescriptorHeap(device,
        {
            .max_descriptor_count = 1,
            .type = RHIDescriptorHeapType::CBV_SRV_UAV_GPU,
            .shader_visible = true
        });
    
    return true;
}

bool DX12DescriptorManager::CreateDescriptor(IRHIDevice& device, const std::shared_ptr<IRHIBuffer>& buffer,
                                             const RHIBufferDescriptorDesc& desc, std::shared_ptr<IRHIBufferDescriptorAllocation>& out_descriptor_allocation)
{
    GLTF_CHECK(desc.m_dimension != RHIResourceDimension::UNKNOWN);
    if (desc.IsVBOrIB())
    {
        out_descriptor_allocation = RHIResourceFactory::CreateRHIResource<IRHIBufferDescriptorAllocation>();
        out_descriptor_allocation->InitFromBuffer(buffer, desc);
        return true;
    }

    // Root buffer descriptor do not allocate within heap
    if ((desc.m_view_type == RHIViewType::RVT_SRV && desc.m_srv_structured_buffer_desc.is_structured_buffer) ||
        desc.m_view_type == RHIViewType::RVT_CBV)
    {
        out_descriptor_allocation = RHIResourceFactory::CreateRHIResource<IRHIBufferDescriptorAllocation>();
        out_descriptor_allocation->InitFromBuffer(buffer, desc);
        auto gpu_handle = dynamic_cast<DX12Buffer&>(*buffer).GetRawBuffer()->GetGPUVirtualAddress();
        dynamic_cast<DX12BufferDescriptorAllocation&>(*out_descriptor_allocation).InitHandle(gpu_handle, 0);
        return true;
    }
    
    return GetDescriptorHeap(desc.m_view_type).CreateResourceDescriptorInHeap(device, buffer, desc, out_descriptor_allocation);
}

bool DX12DescriptorManager::CreateDescriptor(IRHIDevice& device, const std::shared_ptr<IRHITexture>& texture,
                                             const RHITextureDescriptorDesc& desc, std::shared_ptr<IRHITextureDescriptorAllocation>& out_descriptor_allocation)
{
    GLTF_CHECK(desc.m_dimension != RHIResourceDimension::UNKNOWN);
    return GetDescriptorHeap(desc.m_view_type).CreateResourceDescriptorInHeap(device, texture, desc, out_descriptor_allocation);
}

bool DX12DescriptorManager::BindDescriptorContext(IRHICommandList& command_list)
{
    return DX12Utils::DX12Instance().SetDescriptorHeapArray(command_list, m_CBV_SRV_UAV_gpu_heap.get(), 1);
}

bool DX12DescriptorManager::BindGUIDescriptorContext(IRHICommandList& command_list)
{
    return DX12Utils::DX12Instance().SetDescriptorHeapArray(command_list, m_ImGUI_Heap.get(), 1);
}

bool DX12DescriptorManager::Release(glTFRenderResourceManager&)
{
    return true;
}

DX12DescriptorHeap& DX12DescriptorManager::GetDescriptorHeap(RHIViewType type) const
{
    switch (type)
    {
    case RHIViewType::RVT_CBV:
    case RHIViewType::RVT_SRV:
    case RHIViewType::RVT_UAV:
        return GetDescriptorHeap(RHIDescriptorHeapType::CBV_SRV_UAV_GPU);
    case RHIViewType::RVT_RTV:
        return GetDescriptorHeap(RHIDescriptorHeapType::RTV);
    case RHIViewType::RVT_DSV:
        return GetDescriptorHeap(RHIDescriptorHeapType::DSV);
    }
    
    return GetDescriptorHeap(RHIDescriptorHeapType::UNKNOWN);
}

DX12DescriptorHeap& DX12DescriptorManager::GetDescriptorHeap(RHIDescriptorHeapType type) const
{
    switch (type) {
    case RHIDescriptorHeapType::CBV_SRV_UAV_GPU:
        return *m_CBV_SRV_UAV_gpu_heap;
        break;
    case RHIDescriptorHeapType::RTV:
        return *m_RTV_heap;
        break;
    case RHIDescriptorHeapType::DSV:
        return *m_DSV_heap;
        break;
    }

    GLTF_CHECK(false);
    return *m_CBV_SRV_UAV_gpu_heap;
}

DX12DescriptorHeap& DX12DescriptorManager::GetGUIDescriptorHeap() const
{
    return *m_ImGUI_Heap;
}
