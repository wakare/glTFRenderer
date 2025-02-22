#include "RHIResourceFactory.h"

std::vector<std::shared_ptr<IRHIResource>> RHIResourceFactory::resources;

bool RHIResourceFactory::CleanupResources(glTFRenderResourceManager& resource_manager)
{
    LOG_FORMAT_FLUSH("Begin Cleanup %zu Resources\n", resources.size())
    for (int i = resources.size() - 1; i >= 0; i--)
    {
        LOG_FORMAT_FLUSH("CleanupResource %d %s\n", i, resources[i]->GetName().c_str())
        bool released = resources[i]->Release(resource_manager);
            
        GLTF_CHECK(released);
        if (!released)
        {
            return false;
        }
    }

    resources.clear();   
    return true;
}

bool RHIResourceFactory::AddResource(const std::shared_ptr<IRHIResource>& resource)
{
    resources.push_back(resource);
    return true;
}
