#include "RHIInterface/IRHIPipelineStateObject.h"

#include "IRHIRootSignatureHelper.h"
#include "RHIResourceFactoryImpl.hpp"

IRHIPipelineStateObject::IRHIPipelineStateObject(RHIPipelineType type)
    : m_type(type)
    , m_cullMode(RHICullMode::NONE)
    , m_depth_stencil_state(RHIDepthStencilMode::DEPTH_READ)
{
}

bool IRHIPipelineStateObject::BindShaderCode(const std::string& shader_file_path, RHIShaderType type,
    const std::string& entry_function_name)
{
    std::shared_ptr<IRHIShader> shader = RHIResourceFactory::CreateRHIResource<IRHIShader>();
    if (!shader->InitShader(shader_file_path, type, entry_function_name))
    {
        return false;
    }

    // Delay compile shader bytecode util create pso

    assert(m_shaders.find(type) == m_shaders.end());
    m_shaders[type] = shader;
    
    return true;
}

bool IRHIPipelineStateObject::HasBindShader(RHIShaderType type) const
{
    return m_shaders.contains(type);
}

IRHIShader& IRHIPipelineStateObject::GetBindShader(RHIShaderType type)
{
    assert(m_shaders.find(type) != m_shaders.end());
    return *m_shaders[type];
}

RHIShaderPreDefineMacros& IRHIPipelineStateObject::GetShaderMacros()
{
    return m_shader_macros;
}

bool IRHIPipelineStateObject::CompileShaders()
{
    RETURN_IF_FALSE(!m_shaders.empty())

    // Add graphics api shader macro!
    m_shader_macros.AddMacro("DX_SHADER",
        RHIConfigSingleton::Instance().GetGraphicsAPIType() == RHIGraphicsAPIType::RHI_GRAPHICS_API_DX12 ? "1" : "0");

    for (const auto& shader : m_shaders)
    {
        shader.second->SetShaderCompilePreDefineMacros(m_shader_macros);
        RETURN_IF_FALSE(shader.second->CompileShader())
        
        // Debug shader
        IRHIRootSignatureHelper dummy;
        RHIUtilInstanceManager::Instance().RegisterShaderParameterToRootSignature(*shader.second, dummy);
    }
    
    return true;
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

void IRHIRayTracingPipelineStateObject::AddHitGroupDesc(const RHIRayTracingHitGroupDesc& desc)
{
    m_hit_group_descs.push_back(desc);
}

void IRHIRayTracingPipelineStateObject::AddLocalRSDesc(const RHIRayTracingLocalRSDesc& desc)
{
    m_local_rs_descs.push_back(desc);
}

void IRHIRayTracingPipelineStateObject::SetExportFunctionNames(const std::vector<std::string>& export_names)
{
    m_export_function_names = export_names;
}

void IRHIRayTracingPipelineStateObject::SetConfig(const RHIRayTracingConfig& config)
{
    m_config = config;
}

void IRHIPipelineStateObject::SetCullMode(RHICullMode mode)
{
    m_cullMode = mode;
}

void IRHIPipelineStateObject::SetDepthStencilState(RHIDepthStencilMode state)
{
    m_depth_stencil_state = state;
}

RHIDepthStencilMode IRHIPipelineStateObject::GetDepthStencilMode() const
{
    return m_depth_stencil_state;
}

void IRHIPipelineStateObject::SetInputLayouts(const std::vector<RHIPipelineInputLayout>& input_layouts)
{
    m_input_layouts = input_layouts;
}

RHIPipelineType IRHIPipelineStateObject::GetPSOType() const
{
    return m_type;
}

RHICullMode IRHIPipelineStateObject::GetCullMode() const
{
    return m_cullMode;
}
