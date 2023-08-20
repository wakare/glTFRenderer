#pragma once
#include <map>

#include "glTFRenderPassInterfaceSceneView.h"
#include "glTFRenderPassPostprocess.h"
#include "../glTFLoader/glTFElementCommon.h"

struct PointLightInfo
{
    glm::float4 positionAndRadius;
    glm::float4 intensityAndFalloff;
};

struct DirectionalLightInfo
{
    glm::float4 directionalAndIntensity;
};

struct ConstantBufferPerLightDraw
{
    unsigned pointLightCount;
    unsigned directionalLightCount;

    std::vector<PointLightInfo> pointLightInfos;
    std::vector<DirectionalLightInfo> directionalInfos;
};

class glTFRenderPassLighting : public glTFRenderPassPostprocess, public glTFRenderPassInterfaceSceneView
{
    enum
    {
        LightPass_RootParameter_SceneViewCBV = 0,
        LightPass_RootParameter_BaseColorAndDepthSRV = 1,
        LightPass_RootParameter_LightInfosCBV = 2,
        LightPass_RootParameter_PointLightStructuredBuffer = 3,
        LightPass_RootParameter_DirectionalLightStructuredBuffer = 4,
        LightPass_RootParameter_Num,
    };
public:
    glTFRenderPassLighting();

    virtual const char* PassName() override;
    virtual bool InitPass(glTFRenderResourceManager& resourceManager) override;
    virtual bool PreRenderPass(glTFRenderResourceManager& resource_manager) override;
    virtual bool PostRenderPass(glTFRenderResourceManager& resource_manager) override;
    
    virtual bool TryProcessSceneObject(glTFRenderResourceManager& resourceManager, const glTFSceneObjectBase& object) override;
    virtual bool FinishProcessSceneObject(glTFRenderResourceManager& resourceManager) override;
    
protected:
    // Must be implement in final render pass class
    virtual size_t GetMainDescriptorHeapSize() override;
    virtual bool SetupRootSignature(glTFRenderResourceManager& resourceManager) override;
    virtual bool SetupPipelineStateObject(glTFRenderResourceManager& resource_manager) override;

    bool UploadLightInfoGPUBuffer();
    
    std::shared_ptr<IRHIRenderTarget> m_base_pass_color_RT;
    std::shared_ptr<IRHIRenderTarget> m_normal_RT;
    
    RHIGPUDescriptorHandle m_base_pass_color_RT_SRV_Handle;
    RHIGPUDescriptorHandle m_depth_RT_SRV_Handle;
    RHIGPUDescriptorHandle m_normal_RT_SRV_Handle;

    std::map<glTFHandle::HandleIndexType, PointLightInfo> m_cache_point_lights;
    std::map<glTFHandle::HandleIndexType, DirectionalLightInfo> m_cache_directional_lights;
    
    std::shared_ptr<IRHIGPUBuffer> m_light_info_GPU_constant_buffer;
    std::shared_ptr<IRHIGPUBuffer> m_point_light_info_GPU_structured_buffer;
    std::shared_ptr<IRHIGPUBuffer> m_directional_light_info_GPU_structured_buffer;
    
    ConstantBufferPerLightDraw m_constant_buffer_per_light_draw;
};
