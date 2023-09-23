#pragma once
#include "glTFComputePassBase.h"
#include "glTFRenderPassCommon.h"
#include "glTFRenderInterface/glTFRenderInterfaceLighting.h"
#include "glTFRenderInterface/glTFRenderInterfaceSceneView.h"

class glTFComputePassLighting : public glTFComputePassBase, public glTFRenderInterfaceSceneView, public glTFRenderInterfaceLighting
{
public:
    enum glTFComputePassLightingRootParameterIndex
    {
        LightPass_RootParameter_SceneViewCBV                        = 0,
        LightPass_RootParameter_BaseColorAndDepthSRV                = 1,
        LightPass_RootParameter_LightInfosCBV                       = 2,
        LightPass_RootParameter_PointLightStructuredBuffer          = 3,
        LightPass_RootParameter_DirectionalLightStructuredBuffer    = 4,
        LightPass_RootParameter_OutputUAV                           = 5,
        LightPass_RootParameter_Num,
    };
    enum glTFComputePassLightingRegisterIndex
    {
        LightPass_SceneView_CBV_Register        = 0,   
        LightPass_LightInfo_CBV_Register        = 1,
        LightPass_PointLight_SRV_Register       = 3,
        LightPass_DirectionalLight_SRV_Register = 4,
        LightPass_Output_UAV_Register           = 0,
    };
    
    glTFComputePassLighting()
        : glTFRenderInterfaceSceneView(
            LightPass_RootParameter_SceneViewCBV, LightPass_SceneView_CBV_Register)
        , glTFRenderInterfaceLighting(
              LightPass_RootParameter_LightInfosCBV, LightPass_LightInfo_CBV_Register,
              LightPass_RootParameter_PointLightStructuredBuffer, LightPass_PointLight_SRV_Register,
              LightPass_RootParameter_DirectionalLightStructuredBuffer, LightPass_DirectionalLight_SRV_Register)
        , m_base_color_RT(nullptr)
        , m_normal_RT(nullptr)
        , m_base_color_SRV(0)
        , m_depth_SRV(0)
        , m_normal_SRV(0)
        , m_constant_buffer_per_light_draw({})
    {
    }

    virtual const char* PassName() override;
    virtual bool InitPass(glTFRenderResourceManager& resource_manager) override;
    virtual bool PreRenderPass(glTFRenderResourceManager& resource_manager) override;
    virtual bool PostRenderPass(glTFRenderResourceManager& resource_manager) override;

    virtual DispatchCount GetDispatchCount() const override;
    
    virtual bool TryProcessSceneObject(glTFRenderResourceManager& resource_manager, const glTFSceneObjectBase& object) override;
    virtual bool FinishProcessSceneObject(glTFRenderResourceManager& resource_manager) override;
    
protected:
    // Must be implement in final render pass class
    virtual size_t GetRootSignatureParameterCount() override;
    virtual size_t GetRootSignatureSamplerCount() override;
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
};
