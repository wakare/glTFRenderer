#pragma once
#include "glTFRayTracingPassWithMesh.h"

ALIGN_FOR_CBV_STRUCT struct RayTracingDIPassOptions
{
    inline static std::string Name = "RAY_TRACING_DI_OPTION_CBV_INDEX";
    
    BOOL check_visibility_for_all_candidates;
    int candidate_light_count;

    RayTracingDIPassOptions()
        : check_visibility_for_all_candidates(false)
        , candidate_light_count(8)
    {
    }
};

class glTFRayTracingPassReSTIRDirectLighting : public glTFRayTracingPassWithMesh
{
public:
    glTFRayTracingPassReSTIRDirectLighting();
    
    virtual const char* PassName() override;
    virtual bool InitRenderInterface(glTFRenderResourceManager& resource_manager) override;
    virtual bool InitPass(glTFRenderResourceManager& resource_manager) override;
    virtual bool PreRenderPass(glTFRenderResourceManager& resource_manager) override;
    virtual bool PostRenderPass(glTFRenderResourceManager& resource_manager) override;
    virtual bool UpdateGUIWidgets() override;
    
protected:
    virtual bool SetupRootSignature(glTFRenderResourceManager& resource_manager) override;
    virtual bool SetupPipelineStateObject(glTFRenderResourceManager& resource_manager) override;
    
    // Ray function names
    virtual const char* GetRayGenFunctionName() override;
    
    virtual const char* GetPrimaryRayCHFunctionName() override;
    virtual const char* GetPrimaryRayMissFunctionName() override;
    virtual const char* GetPrimaryRayHitGroupName() override;

    virtual const char* GetShadowRayMissFunctionName() override;
    virtual const char* GetShadowRayHitGroupName() override;
    
    virtual bool InitResourceTable(glTFRenderResourceManager& resource_manager) override;
    
private:
    std::shared_ptr<IRHITextureDescriptorAllocation> m_lighting_samples_handle;
    std::shared_ptr<IRHITextureDescriptorAllocation> m_screen_uv_offset_handle;

    RootSignatureAllocation m_lighting_samples_allocation;
    RootSignatureAllocation m_screen_uv_offset_allocation;

    RayTracingDIPassOptions m_pass_options;
};
