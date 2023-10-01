#include "IRHIPipelineStateObject.h"

#include "glTFRHI/RHIResourceFactoryImpl.hpp"

IRHIPipelineStateObject::IRHIPipelineStateObject(RHIPipelineType type)
    : m_type(type)
    , m_cullMode(IRHICullMode::NONE)
    , m_depth_stencil_state(IRHIDepthStencilMode::DEPTH_READ)
{
}

bool IRHIPipelineStateObject::BindShaderCode(const std::string& shaderFilePath, RHIShaderType type,
    const std::string& entryFunctionName)
{
    std::shared_ptr<IRHIShader> dxShader = RHIResourceFactory::CreateRHIResource<IRHIShader>();
    if (!dxShader->InitShader(shaderFilePath, type, entryFunctionName))
    {
        return false;
    }

    // Delay compile shader bytecode util create pso

    assert(m_shaders.find(type) == m_shaders.end());
    m_shaders[type] = dxShader;
    
    return true;
}

IRHIShader& IRHIPipelineStateObject::GetBindShader(RHIShaderType type)
{
    assert(m_shaders.find(type) != m_shaders.end());
    return *m_shaders[type];
}

bool IRHIPipelineStateObject::BindInputLayoutAndSetShaderMacros(const std::vector<RHIPipelineInputLayout>& input_layouts)
{
    RETURN_IF_FALSE(!input_layouts.empty())
    
    m_input_layouts = input_layouts;

    // Add shader pre define macros
    for (const auto& input_layout : m_input_layouts)
    {
        if (input_layout.semanticName == INPUT_LAYOUT_UNIQUE_PARAMETER(NORMAL))
        {
            m_shader_macros.AddMacro("HAS_NORMAL", "1");
        }

        if (input_layout.semanticName == INPUT_LAYOUT_UNIQUE_PARAMETER(TEXCOORD))
        {
            m_shader_macros.AddMacro("HAS_TEXCOORD", "1");
        }

        if (input_layout.semanticName == INPUT_LAYOUT_UNIQUE_PARAMETER(TANGENT))
        {
            m_shader_macros.AddMacro("HAS_TANGENT", "1");
        }
    }
    
    return true;
}

RHIShaderPreDefineMacros& IRHIPipelineStateObject::GetShaderMacros()
{
    return m_shader_macros;
}

IRHIGraphicsPipelineStateObject::IRHIGraphicsPipelineStateObject()
    : IRHIPipelineStateObject(RHIPipelineType::Graphics)
{
    
}

IRHIComputePipelineStateObject::IRHIComputePipelineStateObject()
    : IRHIPipelineStateObject(RHIPipelineType::Compute)
{
}

IRHIRayTracingPipelineStateObject::IRHIRayTracingPipelineStateObject()
    : IRHIPipelineStateObject(RHIPipelineType::RayTracing)
{
}

void IRHIPipelineStateObject::SetCullMode(IRHICullMode mode)
{
    m_cullMode = mode;
}

void IRHIPipelineStateObject::SetDepthStencilState(IRHIDepthStencilMode state)
{
    m_depth_stencil_state = state;
}

IRHICullMode IRHIPipelineStateObject::GetCullMode() const
{
    return m_cullMode;
}
