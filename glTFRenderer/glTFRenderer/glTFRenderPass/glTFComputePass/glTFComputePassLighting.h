#pragma once
#include "glTFComputePassBase.h"
#include "glTFRenderPass/glTFRenderPassCommon.h"
#include "glTFRenderPass/glTFRenderInterface/glTFRenderInterfaceLighting.h"
#include "glTFRenderPass/glTFRenderInterface/glTFRenderInterfaceSceneView.h"

class glTFComputePassLighting : public glTFComputePassBase
{
public:
    glTFComputePassLighting();

    virtual const char* PassName() override;
    virtual bool InitPass(glTFRenderResourceManager& resource_manager) override;
    virtual bool PreRenderPass(glTFRenderResourceManager& resource_manager) override;
    virtual bool PostRenderPass(glTFRenderResourceManager& resource_manager) override;

    virtual DispatchCount GetDispatchCount() const override;
    
    virtual bool TryProcessSceneObject(glTFRenderResourceManager& resource_manager, const glTFSceneObjectBase& object) override;
    virtual bool FinishProcessSceneObject(glTFRenderResourceManager& resource_manager) override;
    
protected:
    virtual size_t GetMainDescriptorHeapSize() override;
    virtual bool SetupRootSignature(glTFRenderResourceManager& resource_manager) override;
    virtual bool SetupPipelineStateObject(glTFRenderResourceManager& resource_manager) override;
    bool UploadLightInfoGPUBuffer();
    
    std::shared_ptr<IRHIRenderTarget> m_base_color_RT;
    std::shared_ptr<IRHIRenderTarget> m_normal_RT;
    std::shared_ptr<IRHIRenderTarget> m_lighting_output_RT;
    
    RHIGPUDescriptorHandle m_base_color_SRV;
    RHIGPUDescriptorHandle m_depth_SRV;
    RHIGPUDescriptorHandle m_normal_SRV;
    RHIGPUDescriptorHandle m_output_UAV;

    std::map<glTFHandle::HandleIndexType, PointLightInfo> m_cache_point_lights;
    std::map<glTFHandle::HandleIndexType, DirectionalLightInfo> m_cache_directional_lights;
    
    ConstantBufferPerLightDraw m_constant_buffer_per_light_draw;
    DispatchCount m_dispatch_count;

    RootSignatureAllocation m_base_color_and_depth_allocation;
    RootSignatureAllocation m_output_allocation;
};
