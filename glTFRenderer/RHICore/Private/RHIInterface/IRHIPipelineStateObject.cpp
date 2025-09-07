#include "RHIInterface/IRHIPipelineStateObject.h"

#include "IRHIRootSignatureHelper.h"
#include "RHIResourceFactoryImpl.hpp"

IRHIPipelineStateObject::IRHIPipelineStateObject(RHIPipelineType type)
    : m_type(type)
    , m_cullMode(RHICullMode::NONE)
    , m_depth_stencil_state(RHIDepthStencilMode::DEPTH_READ)
{
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
