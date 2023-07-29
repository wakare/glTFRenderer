#pragma once
#include "IRHICullMode.h"
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

#define DECLARE_INPUT_LAYOUT_SEMANTIC_NAME(x) static const char* g_inputLayoutName##x = #x;
DECLARE_INPUT_LAYOUT_SEMANTIC_NAME(POSITION)
DECLARE_INPUT_LAYOUT_SEMANTIC_NAME(NORMAL)
DECLARE_INPUT_LAYOUT_SEMANTIC_NAME(TEXCOORD)

#define INPUT_LAYOUT_UNIQUE_PARAMETER(x) (g_inputLayoutName##x)

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
    virtual bool BindShaderCode(const std::string& shaderFilePath, RHIShaderType type, const std::string& entryFunctionName) = 0;
    virtual bool BindRenderTargets(const std::vector<IRHIRenderTarget*>& renderTargets) = 0;
    bool BindInputLayoutAndSetShaderMacros(const std::vector<RHIPipelineInputLayout>& input_layouts);
    virtual bool InitPipelineStateObject(IRHIDevice& device, IRHIRootSignature& rootSignature, IRHISwapChain& swapchain) = 0;

    virtual IRHIShader& GetBindShader(RHIShaderType type) = 0;
    RHIShaderPreDefineMacros& GetShaderMacros();

    void SetCullMode(IRHICullMode mode);
    IRHICullMode GetCullMode() const;
    
protected:
    RHIPipelineType m_type;
    RHIShaderPreDefineMacros m_shaderMacros;
    IRHICullMode m_cullMode;
    std::vector<RHIPipelineInputLayout> m_input_layouts;
};
