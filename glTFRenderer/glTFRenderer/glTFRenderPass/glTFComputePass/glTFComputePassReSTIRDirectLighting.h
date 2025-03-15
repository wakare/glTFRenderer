#pragma once
#include "glTFComputePassBase.h"
#include "glTFRenderPass/glTFRenderResourceUtils.h"

ALIGN_FOR_CBV_STRUCT struct RayTracingDIPostProcessPassOptions
{
    inline static std::string Name = "RAY_TRACING_DI_POSTPROCESS_OPTION_CBV_INDEX";

    int spatial_reuse_range;
    BOOL enable_spatial_reuse;
    BOOL enable_temporal_reuse;
    
    RayTracingDIPostProcessPassOptions()
        : spatial_reuse_range(0)
        , enable_spatial_reuse(false)
        , enable_temporal_reuse(false)
    {
    }
};

class glTFComputePassReSTIRDirectLighting : public glTFComputePassBase
{
public:
    glTFComputePassReSTIRDirectLighting();
    
    virtual const char* PassName() override;
    virtual bool InitRenderInterface(glTFRenderResourceManager& resource_manager) override;
    virtual bool InitPass(glTFRenderResourceManager& resource_manager) override;
    virtual bool PreRenderPass(glTFRenderResourceManager& resource_manager) override;
    virtual bool PostRenderPass(glTFRenderResourceManager& resource_manager) override;

    virtual DispatchCount GetDispatchCount(glTFRenderResourceManager& resource_manager) const override;
    
    virtual bool TryProcessSceneObject(glTFRenderResourceManager& resource_manager, const glTFSceneObjectBase& object) override;
    virtual bool FinishProcessSceneObject(glTFRenderResourceManager& resource_manager) override;
    virtual bool UpdateGUIWidgets() override;
    
protected:
    virtual bool SetupRootSignature(glTFRenderResourceManager& resource_manager) override;
    virtual bool SetupPipelineStateObject(glTFRenderResourceManager& resource_manager) override;
    virtual bool InitResourceTable(glTFRenderResourceManager& resource_manager) override;
    
private:
    RootSignatureAllocation m_output_allocation;
    RootSignatureAllocation m_lighting_samples_allocation;
    RootSignatureAllocation m_screen_uv_offset_allocation;
    
    glTFRenderResourceUtils::RWTextureResourceWithBackBuffer m_aggregate_samples_output;

    RayTracingDIPostProcessPassOptions m_pass_options;
};
