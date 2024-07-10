#include "DX12DescriptorManager.h"

#include "glTFRHI/RHIResourceFactoryImpl.hpp"
#include "glTFRHI/RHIInterface/IRHIDescriptorHeap.h"

bool DX12DescriptorManager::Init(IRHIDevice& device, const RHIMemoryManagerDescriptorMaxCapacity& max_descriptor_capacity)
{
    m_CBV_SRV_UAV_heap = RHIResourceFactory::CreateRHIResource<IRHIDescriptorHeap>();
    m_CBV_SRV_UAV_heap->InitDescriptorHeap(device,
        {
            .max_descriptor_count = max_descriptor_capacity.cbv_srv_uav_size,
            .type = RHIDescriptorHeapType::CBV_SRV_UAV,
            .shader_visible = true
        });

    m_RTV_heap = RHIResourceFactory::CreateRHIResource<IRHIDescriptorHeap>();
    m_RTV_heap->InitDescriptorHeap(device,
        {
            .max_descriptor_count = max_descriptor_capacity.rtv_size,
            .type = RHIDescriptorHeapType::RTV,
            .shader_visible = false
        });

    m_DSV_heap = RHIResourceFactory::CreateRHIResource<IRHIDescriptorHeap>();
    m_DSV_heap->InitDescriptorHeap(device,
        {
            .max_descriptor_count = max_descriptor_capacity.dsv_size,
            .type = RHIDescriptorHeapType::DSV,
            .shader_visible = false
        });

    m_ImGUI_Heap = RHIResourceFactory::CreateRHIResource<IRHIDescriptorHeap>();
    m_ImGUI_Heap->InitDescriptorHeap(device,
        {
            .max_descriptor_count = 1,
            .type = RHIDescriptorHeapType::CBV_SRV_UAV,
            .shader_visible = true
        });
    
    return true;
}

bool DX12DescriptorManager::CreateDescriptor(IRHIDevice& device, IRHIBuffer& buffer,
    const RHIDescriptorDesc& desc, std::shared_ptr<IRHIDescriptorAllocation>& out_descriptor_allocation)
{
    return GetDescriptorHeap(desc.view_type).CreateResourceDescriptorInHeap(device, buffer, desc, out_descriptor_allocation);
}

bool DX12DescriptorManager::CreateDescriptor(IRHIDevice& device, IRHITexture& texture,
    const RHIDescriptorDesc& desc, std::shared_ptr<IRHIDescriptorAllocation>& out_descriptor_allocation)
{
    return GetDescriptorHeap(desc.view_type).CreateResourceDescriptorInHeap(device, texture, desc, out_descriptor_allocation);
}

bool DX12DescriptorManager::CreateDescriptor(IRHIDevice& device, IRHIRenderTarget& texture,
    const RHIDescriptorDesc& desc, std::shared_ptr<IRHIDescriptorAllocation>& out_descriptor_allocation)
{
    return GetDescriptorHeap(desc.view_type).CreateResourceDescriptorInHeap(device, texture, desc, out_descriptor_allocation);
}

bool DX12DescriptorManager::BindDescriptors(IRHICommandList& command_list)
{
    return RHIUtils::Instance().SetDescriptorHeapArray(command_list, m_CBV_SRV_UAV_heap.get(), 1);
}

bool DX12DescriptorManager::BindGUIDescriptors(IRHICommandList& command_list)
{
    return RHIUtils::Instance().SetDescriptorHeapArray(command_list, m_ImGUI_Heap.get(), 1);
}

IRHIDescriptorHeap& DX12DescriptorManager::GetDescriptorHeap(RHIViewType type) const
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

IRHIDescriptorHeap& DX12DescriptorManager::GetDescriptorHeap(RHIDescriptorHeapType type) const
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

IRHIDescriptorHeap& DX12DescriptorManager::GetGUIDescriptorHeap() const
{
    return *m_ImGUI_Heap;
}
