#pragma once
#include "glTFRenderPassBase.h"
#include "../glTFRHI/RHIInterface/IRHIPipelineStateObject.h"
#include "../glTFRHI/RHIInterface/IRHIRenderTargetManager.h"
#include "../glTFRHI/RHIInterface/IRHIRootSignature.h"
#include "../glTFRHI/RHIInterface/IRHIVertexBufferView.h"

class IRHIIndexBufferView;
class IRHIGPUBuffer;

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

    std::shared_ptr<IRHIFence> m_fence;
    
    std::shared_ptr<IRHIRenderTarget> m_depthBuffer;
    std::shared_ptr<IRHIGPUBuffer> m_vertexBuffer;
    std::shared_ptr<IRHIGPUBuffer> m_indexBuffer;
    std::shared_ptr<IRHIGPUBuffer> m_vertexUploadBuffer;
    std::shared_ptr<IRHIGPUBuffer> m_indexUploadBuffer;
    
    std::shared_ptr<IRHIRootSignature> m_rootSignature;
    std::shared_ptr<IRHIPipelineStateObject> m_pipelineStateObject;

    std::shared_ptr<IRHIVertexBufferView> m_vertexBufferView;
    std::shared_ptr<IRHIIndexBufferView> m_indexBufferView;
};
