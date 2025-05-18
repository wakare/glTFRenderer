#pragma once
#include <memory>

#include "RendererCommon.h"
#include "RHIInterface/IRHIResource.h"

class glTFRenderResourceManager;

class RHIResourceFactory
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
