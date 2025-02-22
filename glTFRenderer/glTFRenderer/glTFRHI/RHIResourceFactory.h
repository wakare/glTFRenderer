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
        return std::make_shared<Type>();
    }
    
    static bool CleanupResources(glTFRenderResourceManager& resource_manager);
    
protected:
    static std::vector<IRHIResource*> resources;
};
