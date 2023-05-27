#pragma once
#include "glTFRenderPassInterfaceSceneView.h"
#include "glTFRenderPassPostprocess.h"

struct ConstantBufferPerLightDraw
{
    glm::float4 lightInfos[4];
};

class glTFRenderPassLighting : public glTFRenderPassPostprocess, public glTFRenderPassInterfaceSceneView
{
    enum
    {
        LightPass_RootParameter_SceneView = 0,
        LightPass_RootParameter_BaseColorAndDepthSRV = 1,
        LightPass_RootParameter_LightInfos = 2,
        LightPass_RootParameter_Num,
    };
public:
    glTFRenderPassLighting();

    virtual const char* PassName() override;
    virtual bool InitPass(glTFRenderResourceManager& resourceManager) override;
    virtual bool RenderPass(glTFRenderResourceManager& resourceManager) override;
    virtual bool TryProcessSceneObject(glTFRenderResourceManager& resourceManager, const glTFSceneObjectBase& object) override;
    
protected:
    // Must be implement in final render pass class
    virtual size_t GetMainDescriptorHeapSize() override;
    virtual bool SetupRootSignature(glTFRenderResourceManager& resourceManager) override;
    virtual bool SetupPipelineStateObject(glTFRenderResourceManager& resourceManager) override;

    std::shared_ptr<IRHIRenderTarget> m_basePassColorRT;
    RHIGPUDescriptorHandle m_basePassColorRTSRVHandle;

    RHIGPUDescriptorHandle m_depthRTSRVHandle;

    std::shared_ptr<IRHIGPUBuffer> m_constantBufferInGPU;
    ConstantBufferPerLightDraw m_constantBufferPerLightDraw;
};
