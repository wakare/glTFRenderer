#pragma once
#include <map>
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
DECLARE_INPUT_LAYOUT_SEMANTIC_NAME(TANGENT)
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

enum class IRHICullMode
{
    NONE,
    CW,
    CCW,
};

enum class IRHIDepthStencilMode
{
    DEPTH_READ,
    DEPTH_WRITE,
};

class IRHIPipelineStateObject : public IRHIResource
{
public:
    IRHIPipelineStateObject(RHIPipelineType type);
    
    bool BindShaderCode(const std::string& shaderFilePath, RHIShaderType type, const std::string& entryFunctionName);
    IRHIShader& GetBindShader(RHIShaderType type);
    
    bool BindInputLayoutAndSetShaderMacros(const std::vector<RHIPipelineInputLayout>& input_layouts);
    void SetCullMode(IRHICullMode mode);
    void SetDepthStencilState(IRHIDepthStencilMode state);
    
    IRHICullMode GetCullMode() const;
    RHIShaderPreDefineMacros& GetShaderMacros();
    
protected:
    RHIPipelineType m_type;
    RHIShaderPreDefineMacros m_shader_macros;
    IRHICullMode m_cullMode;
    IRHIDepthStencilMode m_depth_stencil_state;
    std::vector<RHIPipelineInputLayout> m_input_layouts;
    std::map<RHIShaderType, std::shared_ptr<IRHIShader>> m_shaders;
};

class IRHIGraphicsPipelineStateObject : public IRHIPipelineStateObject
{
public:
    IRHIGraphicsPipelineStateObject();
    
    virtual bool BindRenderTargets(const std::vector<IRHIRenderTarget*>& render_targets) = 0;
    virtual bool InitGraphicsPipelineStateObject(IRHIDevice& device, IRHIRootSignature& root_signature, IRHISwapChain& swapchain) = 0;
};

class IRHIComputePipelineStateObject : public IRHIPipelineStateObject
{
public:
    IRHIComputePipelineStateObject();

    virtual bool InitComputePipelineStateObject(IRHIDevice& device, IRHIRootSignature& root_signature) = 0; 
};

class IRHIRayTracingPipelineStateObject : public IRHIPipelineStateObject
{
public:
    IRHIRayTracingPipelineStateObject();

    virtual bool InitRayTracingPipelineStateObject(IRHIDevice& device, IRHIRootSignature& root_signature) = 0;
};