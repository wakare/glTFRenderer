#pragma once
#include "IRHIDevice.h"
#include "IRHIRootSignature.h"
#include "IRHIResource.h"
#include "IRHIShader.h"
#include "IRHISwapChain.h"

class IRHIRenderTarget;

enum class RHIPipelineType
{
    Graphics,
    Compute,
    //Copy,
    Unknown,
};

struct RHIPipelineInputLayout
{
    std::string semanticName;
    unsigned semanticIndex;
    RHIDataFormat format;
    unsigned alignedByteOffset;

    // TODO: support instancing data layout
};

class IRHIPipelineStateObject : public IRHIResource
{
public:
    IRHIPipelineStateObject(RHIPipelineType type);
    virtual bool BindShaderCode(const std::string& shaderFilePath, RHIShaderType type) = 0;
    virtual bool BindRenderTargets(const std::vector<IRHIRenderTarget*>& renderTargets) = 0;
    virtual bool InitPipelineStateObject(IRHIDevice& device, IRHIRootSignature& rootSignature, IRHISwapChain& swapchain, const std::vector<RHIPipelineInputLayout>& inputLayouts) = 0;

    virtual IRHIShader& GetBindShader(RHIShaderType type) = 0;
    
protected:
    RHIPipelineType m_type;
};
