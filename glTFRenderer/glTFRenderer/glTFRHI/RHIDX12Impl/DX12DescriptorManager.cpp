#include "DX12DescriptorManager.h"

#include "glTFRHI/RHIResourceFactoryImpl.hpp"
#include "DX12DescriptorHeap.h"

bool DX12DescriptorAllocation::InitFromBuffer(const IRHIBuffer& buffer, const RHIDescriptorDesc& desc)
{
    m_gpu_handle = dynamic_cast<const DX12Buffer&>(buffer).GetBuffer()->GetGPUVirtualAddress();
    m_view_desc = desc;
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


bool DX12DescriptorManager::Init(IRHIDevice& device, const RHIMemoryManagerDescriptorMaxCapacity& max_descriptor_capacity)
{
    m_CBV_SRV_UAV_heap = std::make_shared<DX12DescriptorHeap>();
    m_CBV_SRV_UAV_heap->InitDescriptorHeap(device,
        {
            .max_descriptor_count = max_descriptor_capacity.cbv_srv_uav_size,
            .type = RHIDescriptorHeapType::CBV_SRV_UAV,
            .shader_visible = true
        });

    m_RTV_heap = std::make_shared<DX12DescriptorHeap>();
    m_RTV_heap->InitDescriptorHeap(device,
        {
            .max_descriptor_count = max_descriptor_capacity.rtv_size,
            .type = RHIDescriptorHeapType::RTV,
            .shader_visible = false
        });

    m_DSV_heap = std::make_shared<DX12DescriptorHeap>();
    m_DSV_heap->InitDescriptorHeap(device,
        {
            .max_descriptor_count = max_descriptor_capacity.dsv_size,
            .type = RHIDescriptorHeapType::DSV,
            .shader_visible = false
        });

    m_ImGUI_Heap = std::make_shared<DX12DescriptorHeap>();
    m_ImGUI_Heap->InitDescriptorHeap(device,
        {
            .max_descriptor_count = 1,
            .type = RHIDescriptorHeapType::CBV_SRV_UAV,
            .shader_visible = true
        });
    
    return true;
}

bool DX12DescriptorManager::CreateDescriptor(IRHIDevice& device, const IRHIBuffer& buffer,
                                             const RHIDescriptorDesc& desc, std::shared_ptr<IRHIDescriptorAllocation>& out_descriptor_allocation)
{
    GLTF_CHECK(desc.dimension != RHIResourceDimension::UNKNOWN);
    return GetDescriptorHeap(desc.view_type).CreateResourceDescriptorInHeap(device, buffer, desc, out_descriptor_allocation);
}

bool DX12DescriptorManager::CreateDescriptor(IRHIDevice& device, const IRHITexture& texture,
                                             const RHIDescriptorDesc& desc, std::shared_ptr<IRHIDescriptorAllocation>& out_descriptor_allocation)
{
    GLTF_CHECK(desc.dimension != RHIResourceDimension::UNKNOWN);
    return GetDescriptorHeap(desc.view_type).CreateResourceDescriptorInHeap(device, texture, desc, out_descriptor_allocation);
}

bool DX12DescriptorManager::CreateDescriptor(IRHIDevice& device, const IRHIRenderTarget& texture,
                                             const RHIDescriptorDesc& desc, std::shared_ptr<IRHIDescriptorAllocation>& out_descriptor_allocation)
{
    GLTF_CHECK(desc.dimension != RHIResourceDimension::UNKNOWN);
    return GetDescriptorHeap(desc.view_type).CreateResourceDescriptorInHeap(device, texture, desc, out_descriptor_allocation);
}

bool DX12DescriptorManager::BindDescriptors(IRHICommandList& command_list)
{
    return DX12Utils::DX12Instance().SetDescriptorHeapArray(command_list, m_CBV_SRV_UAV_heap.get(), 1);
}

bool DX12DescriptorManager::BindGUIDescriptors(IRHICommandList& command_list)
{
    return DX12Utils::DX12Instance().SetDescriptorHeapArray(command_list, m_ImGUI_Heap.get(), 1);
}

DX12DescriptorHeap& DX12DescriptorManager::GetDescriptorHeap(RHIViewType type) const
{
    switch (type)
    {
    case RHIViewType::RVT_CBV:
    case RHIViewType::RVT_SRV:
    case RHIViewType::RVT_UAV:
        return GetDescriptorHeap(RHIDescriptorHeapType::CBV_SRV_UAV);
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
    case RHIDescriptorHeapType::CBV_SRV_UAV:
        return *m_CBV_SRV_UAV_heap;
        break;
    case RHIDescriptorHeapType::RTV:
        return *m_RTV_heap;
        break;
    case RHIDescriptorHeapType::DSV:
        return *m_DSV_heap;
        break;
    }

    GLTF_CHECK(false);
    return *m_CBV_SRV_UAV_heap;
}

DX12DescriptorHeap& DX12DescriptorManager::GetGUIDescriptorHeap() const
{
    return *m_ImGUI_Heap;
}
