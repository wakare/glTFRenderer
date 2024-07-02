#pragma once
#include "glTFComputePassBase.h"

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
    virtual bool InitPass(glTFRenderResourceManager& resource_manager) override;
    virtual bool PreRenderPass(glTFRenderResourceManager& resource_manager) override;
    virtual bool PostRenderPass(glTFRenderResourceManager& resource_manager) override;

    virtual DispatchCount GetDispatchCount() const override;
    
    virtual bool TryProcessSceneObject(glTFRenderResourceManager& resource_manager, const glTFSceneObjectBase& object) override;
    virtual bool FinishProcessSceneObject(glTFRenderResourceManager& resource_manager) override;
    virtual bool UpdateGUIWidgets() override;
    
protected:
    virtual bool SetupRootSignature(glTFRenderResourceManager& resource_manager) override;
    virtual bool SetupPipelineStateObject(glTFRenderResourceManager& resource_manager) override;

private:
    DispatchCount m_dispatch_count;
    
    std::shared_ptr<IRHIRenderTarget> m_output;
    RootSignatureAllocation m_output_allocation;
    std::shared_ptr<IRHIDescriptorAllocation> m_output_handle;

    glTFRenderResourceUtils::RWTextureResourceWithBackBuffer m_aggregate_samples_output;
    
    // External resource
    std::shared_ptr<IRHIRenderTarget> m_lighting_samples;
    std::shared_ptr<IRHIRenderTarget> m_screen_uv_offset;

    std::shared_ptr<IRHIDescriptorAllocation> m_lighting_samples_handle;
    std::shared_ptr<IRHIDescriptorAllocation> m_screen_uv_offset_handle;
    
    RootSignatureAllocation m_lighting_samples_allocation;
    RootSignatureAllocation m_screen_uv_offset_allocation;

    RayTracingDIPostProcessPassOptions m_pass_options;
};
