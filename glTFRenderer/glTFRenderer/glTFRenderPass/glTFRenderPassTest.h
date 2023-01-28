#pragma once
#include "glTFRenderPassBase.h"
#include "../glTFRHI/RHIInterface/IRHIPipelineStateObject.h"
#include "../glTFRHI/RHIInterface/IRHIRenderTargetManager.h"
#include "../glTFRHI/RHIInterface/IRHIRootSignature.h"

// RenderPassTest only draw single triangle for debug RHI code 
class glTFRenderPassTest : public glTFRenderPassBase
{
public:
    glTFRenderPassTest();
    
    virtual const char* PassName() override;
    virtual bool InitPass(glTFRenderResourceManager& resourceManager) override;
    virtual bool RenderPass(glTFRenderResourceManager& resourceManager) override;

protected:
    size_t m_rootSignatureParameterCount;
    size_t m_rootSignatureStaticSamplerCount;

    std::shared_ptr<IRHIRenderTarget> m_depthBuffer;
    
    std::shared_ptr<IRHIRootSignature> m_rootSignature;
    std::shared_ptr<IRHIPipelineStateObject> m_pipelineStateObject;
};
