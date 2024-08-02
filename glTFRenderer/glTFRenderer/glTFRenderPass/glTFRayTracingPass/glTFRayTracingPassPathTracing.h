#pragma once
#include "glTFRayTracingPassBase.h"
#include "glTFRayTracingPassWithMesh.h"

ALIGN_FOR_CBV_STRUCT struct RayTracingPathTracingPassOptions
{
    inline static std::string Name = "RAY_TRACING_PATH_TRACING_OPTION_CBV_INDEX";
    
    int max_bounce_count;
    int candidate_light_count;
    int samples_per_pixel;
    BOOL check_visibility_for_all_candidates;
    BOOL ris_light_sampling;
    BOOL debug_radiosity;
    
    RayTracingPathTracingPassOptions()
        : max_bounce_count(2)
        , candidate_light_count(8)
        , samples_per_pixel(1)
        , check_visibility_for_all_candidates(true)
        , ris_light_sampling(true)
        , debug_radiosity(false)
    {
    }
};

class glTFRayTracingPassPathTracing : public glTFRayTracingPassWithMesh
{
public:
    glTFRayTracingPassPathTracing();

    virtual const char* PassName() override;
    virtual bool InitRenderInterface(glTFRenderResourceManager& resource_manager) override;
    virtual bool InitPass(glTFRenderResourceManager& resource_manager) override;
    virtual bool PreRenderPass(glTFRenderResourceManager& resource_manager) override;
    virtual bool PostRenderPass(glTFRenderResourceManager& resource_manager) override;

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

    virtual bool UpdateGUIWidgets() override;
    virtual bool InitResourceTable(glTFRenderResourceManager& resource_manager) override;
    
private:
    std::shared_ptr<IRHITextureDescriptorAllocation> m_raytracing_output_handle;
    std::shared_ptr<IRHITextureDescriptorAllocation> m_screen_uv_offset_handle;

    RootSignatureAllocation m_output_allocation;
    RootSignatureAllocation m_screen_uv_offset_allocation;

    RayTracingPathTracingPassOptions m_pass_options;
};
