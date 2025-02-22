#include "IRHIMemoryManager.h"

#include "glTFRenderPass/glTFRenderResourceManager.h"
#include "glTFRHI/RHIResourceFactoryImpl.hpp"

bool IRHIMemoryAllocation::Release(glTFRenderResourceManager& resource_manager)
{
    if (!need_release)
    {
        return true;
    }

    need_release = false;
    return resource_manager.GetMemoryManager().ReleaseMemoryAllocation(resource_manager, *this);
}

bool IRHIMemoryManager::InitMemoryManager(IRHIDevice& device,
                                          const IRHIFactory& factory, const DescriptorAllocationInfo& descriptor_allocation_info)
{
    m_descriptor_manager = RHIResourceFactory::CreateRHIResource<IRHIDescriptorManager>();
    m_descriptor_manager->Init(device, descriptor_allocation_info);
    
    return true;
}

IRHIDescriptorManager& IRHIMemoryManager::GetDescriptorManager() const
{
    return *m_descriptor_manager;
}
