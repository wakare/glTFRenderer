#pragma once
#include "glTFRayTracingPassWithMesh.h"
#include "glTFRenderPass/glTFRenderResourceUtils.h"

class glTFRayTracingPassReSTIRDirectLighting : public glTFRayTracingPassWithMesh
{
public:
    glTFRayTracingPassReSTIRDirectLighting();
    
    virtual const char* PassName() override;
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
    
private:
    std::shared_ptr<IRHIRenderTarget> m_lighting_samples;
    std::shared_ptr<IRHIRenderTarget> m_screen_uv_offset_output;
    
    RHIGPUDescriptorHandle m_lighting_samples_handle;
    RHIGPUDescriptorHandle m_screen_uv_offset_handle;

    RootSignatureAllocation m_lighting_samples_allocation;
    RootSignatureAllocation m_screen_uv_offset_allocation;
};