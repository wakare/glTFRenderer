#pragma once
#include "glTFRenderPassPostprocess.h"

class glTFRenderPassLighting : public glTFRenderPassPostprocess
{
public:
    glTFRenderPassLighting();

    virtual const char* PassName() override;
    virtual bool RenderPass(glTFRenderResourceManager& resourceManager) override;
    virtual bool TryProcessSceneObject(glTFRenderResourceManager& resourceManager, const glTFSceneObjectBase& object) override;
    
protected:
    // Must be implement in final render pass class
    virtual size_t GetMainDescriptorHeapSize() override;
    virtual bool SetupRootSignature(glTFRenderResourceManager& resourceManager) override;
    virtual bool SetupPipelineStateObject(glTFRenderResourceManager& resourceManager) override;

    std::shared_ptr<IRHIRenderTarget> m_basePassColorRT;
    RHIGPUDescriptorHandle m_basePassColorRTSRVHandle;
};
