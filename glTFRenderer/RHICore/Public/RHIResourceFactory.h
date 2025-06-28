#pragma once
#include <memory>

#include "RendererCommon.h"
#include "RHIInterface/IRHIResource.h"

class glTFRenderResourceManager;

class RHICORE_API RHIResourceFactory
{
public:
    // Actually create rhi class instance based RHI Graphics API type
    template<typename Type>
    static std::shared_ptr<Type> CreateRHIResource()
    {
        GLTF_CHECK(false);
        return nullptr;
    }
    
    static bool CleanupResources(IRHIMemoryManager& memory_manager);
    static bool AddResource(const std::shared_ptr<IRHIResource>& resource);
    
protected:
    static std::vector<std::shared_ptr<IRHIResource>> resources;
};

inline bool RHIResourceFactory::CleanupResources(IRHIMemoryManager& memory_manager)
{
    LOG_FORMAT_FLUSH("Begin Cleanup %zu Resources\n", resources.size())
    for (int i = resources.size() - 1; i >= 0; i--)
    {
        bool released = resources[i]->Release(memory_manager);
            
        GLTF_CHECK(released);
        if (!released)
        {
            return false;
        }
    }

    resources.clear();   
    return true;
}

inline bool RHIResourceFactory::AddResource(const std::shared_ptr<IRHIResource>& resource)
{
    resources.push_back(resource);
    return true;
}
