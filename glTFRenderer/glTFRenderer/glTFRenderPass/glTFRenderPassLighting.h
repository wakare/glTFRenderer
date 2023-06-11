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
    virtual bool RenderPass(glTFRenderResourceManager& resourceManager) override;
    
    virtual bool TryProcessSceneObject(glTFRenderResourceManager& resourceManager, const glTFSceneObjectBase& object) override;
    virtual bool FinishProcessSceneObject(glTFRenderResourceManager& resourceManager) override;
    
protected:
    // Must be implement in final render pass class
    virtual size_t GetMainDescriptorHeapSize() override;
    virtual bool SetupRootSignature(glTFRenderResourceManager& resourceManager) override;
    virtual bool SetupPipelineStateObject(glTFRenderResourceManager& resourceManager) override;

    bool UploadLightInfoGPUBuffer();
    
    std::shared_ptr<IRHIRenderTarget> m_basePassColorRT;
    std::shared_ptr<IRHIRenderTarget> m_normalRT;
    
    RHIGPUDescriptorHandle m_basePassColorRTSRVHandle;
    RHIGPUDescriptorHandle m_depthRTSRVHandle;
    RHIGPUDescriptorHandle m_normalRTSRVHandle;

    std::map<glTFHandle::HandleIndexType, PointLightInfo> m_cachePointLights;
    std::map<glTFHandle::HandleIndexType, DirectionalLightInfo> m_cacheDirectionalLights;
    
    std::shared_ptr<IRHIGPUBuffer> m_lightInfoGPUConstantBuffer;
    std::shared_ptr<IRHIGPUBuffer> m_pointLightInfoGPUStructuredBuffer;
    std::shared_ptr<IRHIGPUBuffer> m_directionalLightInfoGPUStructuredBuffer;
    
    ConstantBufferPerLightDraw m_constantBufferPerLightDraw;
};
