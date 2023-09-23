#pragma once
#include <map>

#include "glTFRenderInterface/glTFRenderInterfaceSceneView.h"
#include "glTFGraphicsPassPostprocess.h"
#include "glTFRenderPassCommon.h"
#include "../glTFLoader/glTFElementCommon.h"
#include "glTFRenderInterface/glTFRenderInterfaceLighting.h"

class glTFGraphicsPassLighting : public glTFGraphicsPassPostprocess, public glTFRenderInterfaceSceneView, public glTFRenderInterfaceLighting
{
    enum glTFGraphicsPassLightingRootParameterIndex
    {
        LightPass_RootParameter_SceneViewCBV                        = 0,
        LightPass_RootParameter_BaseColorAndDepthSRV                = 1,
        LightPass_RootParameter_LightInfosCBV                       = 2,
        LightPass_RootParameter_PointLightStructuredBuffer          = 3,
        LightPass_RootParameter_DirectionalLightStructuredBuffer    = 4,
        LightPass_RootParameter_Num,
    };
    enum glTFGraphicsPassLightingRegisterIndex
    {
        LightPass_SceneView_CBV_Register = 0,   
        LightPass_LightInfo_CBV_Register = 1,
        LightPass_PointLight_SRV_Register = 3,
        LightPass_DirectionalLight_SRV_Register = 4,
    };
public:
    glTFGraphicsPassLighting();

    virtual const char* PassName() override;
    virtual bool InitPass(glTFRenderResourceManager& resource_manager) override;
    virtual bool PreRenderPass(glTFRenderResourceManager& resource_manager) override;
    virtual bool PostRenderPass(glTFRenderResourceManager& resource_manager) override;
    
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
    
    std::shared_ptr<IRHIRenderTarget> m_base_pass_color_RT;
    std::shared_ptr<IRHIRenderTarget> m_normal_RT;
    
    RHIGPUDescriptorHandle m_base_pass_color_RT_SRV_Handle;
    RHIGPUDescriptorHandle m_depth_RT_SRV_Handle;
    RHIGPUDescriptorHandle m_normal_RT_SRV_Handle;

    std::map<glTFHandle::HandleIndexType, PointLightInfo> m_cache_point_lights;
    std::map<glTFHandle::HandleIndexType, DirectionalLightInfo> m_cache_directional_lights;
    
    ConstantBufferPerLightDraw m_constant_buffer_per_light_draw;
};
