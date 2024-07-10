#include "IRHIMemoryManager.h"

#include "glTFRHI/RHIResourceFactoryImpl.hpp"

bool IRHIMemoryManager::InitMemoryManager(IRHIDevice& device, const std::shared_ptr<IRHIMemoryAllocator>& memory_allocator,
                                          const RHIMemoryManagerDescriptorMaxCapacity& max_descriptor_capacity)
{
    m_descriptor_manager = RHIResourceFactory::CreateRHIResource<IRHIDescriptorManager>();
    m_descriptor_manager->Init(device, max_descriptor_capacity);

    m_allocator = memory_allocator;
    
    return true;
}

IRHIDescriptorManager& IRHIMemoryManager::GetDescriptorManager() const
{
    return *m_descriptor_manager;
}
