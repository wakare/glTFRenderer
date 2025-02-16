#pragma once
#include "glTFComputePassBase.h"
#include "glTFRenderPass/glTFRenderResourceUtils.h"

ALIGN_FOR_CBV_STRUCT struct RayTracingPostProcessPassOptions
{
    inline static std::string Name = "RAY_TRACING_POSTPROCESS_OPTION_CBV_INDEX";

    BOOL enable_post_process;
    BOOL use_velocity_clamp;
    float reuse_history_factor;
    int color_clamp_range;
    
    RayTracingPostProcessPassOptions()
        : enable_post_process(false)
        , use_velocity_clamp(false)
        , reuse_history_factor(0.97f)
        , color_clamp_range(0)
    {
    }
};

class glTFComputePassRayTracingPostprocess : public glTFComputePassBase
{
public:
    glTFComputePassRayTracingPostprocess();

    virtual const char* PassName() override;
    virtual bool InitRenderInterface(glTFRenderResourceManager& resource_manager) override;
    virtual bool InitPass(glTFRenderResourceManager& resource_manager) override;
    virtual bool PreRenderPass(glTFRenderResourceManager& resource_manager) override;
    virtual bool PostRenderPass(glTFRenderResourceManager& resource_manager) override;

    virtual DispatchCount GetDispatchCount() const override;
    
    virtual bool TryProcessSceneObject(glTFRenderResourceManager& resource_manager, const glTFSceneObjectBase& object) override;
    virtual bool FinishProcessSceneObject(glTFRenderResourceManager& resource_manager) override;
    virtual bool UpdateGUIWidgets() override;

    virtual bool InitResourceTable(glTFRenderResourceManager& resource_manager) override;
    
protected:
    virtual bool SetupRootSignature(glTFRenderResourceManager& resource_manager) override;
    virtual bool SetupPipelineStateObject(glTFRenderResourceManager& resource_manager) override;

    DispatchCount m_dispatch_count;

    glTFRenderResourceUtils::RWTextureResourceWithBackBuffer m_accumulation_resource;
    glTFRenderResourceUtils::RWTextureResourceWithBackBuffer m_custom_resource;

    RootSignatureAllocation m_process_output_allocation;
    RootSignatureAllocation m_process_input_allocation;
    RootSignatureAllocation m_screen_uv_offset_allocation;

    RayTracingPostProcessPassOptions m_pass_options;
};

