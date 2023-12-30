#pragma once
#include "glTFRayTracingPassBase.h"

class glTFRayTracingPassWithMesh : public glTFRayTracingPassBase
{
public:
    glTFRayTracingPassWithMesh();
    
    virtual bool InitPass(glTFRenderResourceManager& resource_manager) override;
    virtual bool PreRenderPass(glTFRenderResourceManager& resource_manager) override;
    virtual bool PostRenderPass(glTFRenderResourceManager& resource_manager) override;
    
    virtual bool TryProcessSceneObject(glTFRenderResourceManager& resource_manager, const glTFSceneObjectBase& object) override;
    
protected:
    virtual bool SetupRootSignature(glTFRenderResourceManager& resource_manager) override;
    virtual bool SetupPipelineStateObject(glTFRenderResourceManager& resource_manager) override;
    
    bool UpdateAS(glTFRenderResourceManager& resource_manager);
    bool BuildAS(glTFRenderResourceManager& resource_manager);

    virtual const char* GetRayGenFunctionName() = 0;
    
    virtual const char* GetPrimaryRayCHFunctionName() = 0;
    virtual const char* GetPrimaryRayMissFunctionName() = 0;
    virtual const char* GetPrimaryRayHitGroupName() = 0;

    virtual const char* GetShadowRayMissFunctionName() = 0;
    virtual const char* GetShadowRayHitGroupName() = 0;
    
    std::shared_ptr<IRHIRayTracingAS> m_raytracing_as;
};
