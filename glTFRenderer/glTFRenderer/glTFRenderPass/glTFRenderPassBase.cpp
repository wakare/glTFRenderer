#include "glTFRenderPassBase.h"

#include <imgui.h>

#include "glTFRenderResourceManager.h"
#include "glTFRHI/RHIResourceFactoryImpl.hpp"

bool glTFRenderPassBase::InitRenderInterface(glTFRenderResourceManager& resource_manager)
{
    return true;
}

bool glTFRenderPassBase::InitPass(glTFRenderResourceManager& resource_manager)
{
    m_descriptor_updater = RHIResourceFactory::CreateRHIResource<IRHIDescriptorUpdater>();

    switch (GetPipelineType())
    {
    case RHIPipelineType::Graphics:
        m_pipeline_state_object = RHIResourceFactory::CreateRHIResource<IRHIGraphicsPipelineStateObject>();
        break;
    case RHIPipelineType::Compute:
        m_pipeline_state_object = RHIResourceFactory::CreateRHIResource<IRHIComputePipelineStateObject>();
        break;
    case RHIPipelineType::RayTracing:
        m_pipeline_state_object = RHIResourceFactory::CreateRHIResource<IRHIRayTracingPipelineStateObject>();
        break;
    default: GLTF_CHECK(false);
    }

    // Init root signature
    m_root_signature_helper.SetUsage(RHIRootSignatureUsage::Default);
    RETURN_IF_FALSE(SetupRootSignature(resource_manager))
    RETURN_IF_FALSE(m_root_signature_helper.BuildRootSignature(resource_manager.GetDevice(), resource_manager))
    
    RETURN_IF_FALSE(SetupPipelineStateObject(resource_manager))
    RETURN_IF_FALSE(m_pipeline_state_object->InitPipelineStateObject(resource_manager.GetDevice(),
            m_root_signature_helper.GetRootSignature(),
            resource_manager.GetSwapChain(),
            GetVertexStreamingManager(resource_manager).GetVertexAttributes()
        ))

    for (const auto& render_interface : m_render_interfaces)
    {
        RETURN_IF_FALSE(render_interface->InitInterface(resource_manager))    
    }
    
    return true;
}

bool glTFRenderPassBase::PreRenderPass(glTFRenderResourceManager& resource_manager)
{
    resource_manager.SetCurrentPSO(m_pipeline_state_object);
    
    auto& command_list = resource_manager.GetCommandListForRecord();
    
    RETURN_IF_FALSE(resource_manager.GetMemoryManager().GetDescriptorManager().BindDescriptorContext(command_list))
    
    RETURN_IF_FALSE(RHIUtils::Instance().SetRootSignature(command_list, m_root_signature_helper.GetRootSignature(), *m_pipeline_state_object, GetPipelineType()))

    for (const auto& render_interface : m_render_interfaces)
    {
        RETURN_IF_FALSE(render_interface->ApplyInterface(resource_manager, GetPipelineType(), *m_descriptor_updater))    
    }

    m_begin_rendering_info.rendering_area_width = resource_manager.GetSwapChain().GetWidth();
    m_begin_rendering_info.rendering_area_height = resource_manager.GetSwapChain().GetHeight();
    m_begin_rendering_info.clear_render_target = false;
    m_begin_rendering_info.clear_depth = false;
    
    return true;
}

bool glTFRenderPassBase::RenderPass(glTFRenderResourceManager& resource_manager)
{
    RETURN_IF_FALSE(m_descriptor_updater->FinalizeUpdateDescriptors(resource_manager.GetDevice(), resource_manager.GetCommandListForRecord(), m_root_signature_helper.GetRootSignature()))
    
    RHIUtils::Instance().BeginRendering(resource_manager.GetCommandListForRecord(), m_begin_rendering_info);

    return true;
}

bool glTFRenderPassBase::PostRenderPass(glTFRenderResourceManager& resource_manager)
{
    RHIUtils::Instance().EndRendering(resource_manager.GetCommandListForRecord());
    
    return true;
}

bool glTFRenderPassBase::UpdateGUIWidgets()
{
    return true;
}

bool glTFRenderPassBase::SetupRootSignature(glTFRenderResourceManager& resource_manager)
{
    for (const auto& render_interface : m_render_interfaces)
    {
        RETURN_IF_FALSE(render_interface->ApplyRootSignature(m_root_signature_helper))    
    }
    
    return true;
}

bool glTFRenderPassBase::SetupPipelineStateObject(glTFRenderResourceManager& resource_manager)
{
    auto& shader_macros = m_pipeline_state_object->GetShaderMacros();
    GetVertexStreamingManager(resource_manager).ConfigShaderMacros(shader_macros);
    for (const auto& render_interface : m_render_interfaces)
    {
        render_interface->ApplyShaderDefine(shader_macros);    
    }
    
    return true;
}

void glTFRenderPassBase::AddRenderInterface(const std::shared_ptr<glTFRenderInterfaceBase>& render_interface)
{
    m_render_interfaces.push_back(render_interface);
}

bool glTFRenderPassBase::BindDescriptor(IRHICommandList& command_list, const RootSignatureAllocation& root_signature_allocation,
    const IRHIDescriptorAllocation& allocation) const
{
    return m_descriptor_updater->BindDescriptor(command_list,
            GetPipelineType(),
            root_signature_allocation,
            allocation);
}

const RHIVertexStreamingManager& glTFRenderPassBase::GetVertexStreamingManager(
    glTFRenderResourceManager& resource_manager) const
{
    static RHIVertexStreamingManager _internal_vertex_streaming_manager;
    return _internal_vertex_streaming_manager;
}
