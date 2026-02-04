#include "glTFRenderPassBase.h"

#include <imgui.h>

#include "glTFRenderResourceManager.h"
#include "RHIResourceFactoryImpl.hpp"

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
    RETURN_IF_FALSE(m_root_signature_helper.BuildRootSignature(resource_manager.GetDevice(), resource_manager.GetMemoryManager().GetDescriptorManager()))
    
    RETURN_IF_FALSE(SetupPipelineStateObject(resource_manager))
    RETURN_IF_FALSE(CompileShaders())
    RETURN_IF_FALSE(m_pipeline_state_object->InitPipelineStateObject(resource_manager.GetDevice(),
            m_root_signature_helper.GetRootSignature(),
            resource_manager.GetSwapChain(),m_shaders
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
    
    RETURN_IF_FALSE(RHIUtilInstanceManager::Instance().SetRootSignature(command_list, m_root_signature_helper.GetRootSignature(), *m_pipeline_state_object, GetPipelineType()))

    for (const auto& render_interface : m_render_interfaces)
    {
        RETURN_IF_FALSE(render_interface->ApplyInterface(resource_manager, GetPipelineType(), *m_descriptor_updater))    
    }

    return true;
}

bool glTFRenderPassBase::RenderPass(glTFRenderResourceManager& resource_manager)
{
    RETURN_IF_FALSE(m_descriptor_updater->FinalizeUpdateDescriptors(resource_manager.GetDevice(), resource_manager.GetCommandListForRecord(), m_root_signature_helper.GetRootSignature()))
    
    //RHIUtilInstanceManager::Instance().BeginRendering(resource_manager.GetCommandListForRecord(), m_begin_rendering_info);

    return true;
}

bool glTFRenderPassBase::PostRenderPass(glTFRenderResourceManager& resource_manager)
{
    //RHIUtilInstanceManager::Instance().EndRendering(resource_manager.GetCommandListForRecord());
    
    return true;
}

bool glTFRenderPassBase::UpdateGUIWidgets()
{
    ImGui::Checkbox("Rendering Enabled", &m_rendering_enabled);

    if (!m_final_output_candidates.empty())
    {
        constexpr ImVec4 subtitle_color = {1.0f, 0.0f, 1.0f, 1.0f};
        ImGui::TextColored(subtitle_color, "Final Output");
        for (int i = 0; i < m_final_output_candidates.size(); ++i)
        {
            auto label = RenderPassResourceTableName(m_final_output_candidates[i]);
            if (ImGui::RadioButton(label.c_str(), m_final_output_index == i))
            {
                m_final_output_index = i;
            }
        }    
    }
    
    return true;
}

bool glTFRenderPassBase::ModifyFinalOutput(RenderGraphNodeUtil::RenderGraphNodeFinalOutput& final_output)
{
    RETURN_IF_FALSE(RenderGraphNode::ModifyFinalOutput(final_output))
    
    if (!m_final_output_candidates.empty())
    {
        auto output_texture = GetResourceTexture(m_final_output_candidates[m_final_output_index]);
        // TODO: Match texture byte with swapchain real format
        if (GetBytePerPixelByFormat(output_texture->GetTextureFormat()) == 4 && !output_texture->GetTextureDesc().HasUsage(RHIResourceUsageFlags::RUF_ALLOW_DEPTH_STENCIL))
        {
            final_output.final_color_output = GetResourceTexture(m_final_output_candidates[m_final_output_index]);    
        }
    }
    
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
    auto& shader_macros = GetShaderMacros();
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

bool glTFRenderPassBase::BindShaderCode(const std::string& shader_file_path, RHIShaderType type,
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

bool glTFRenderPassBase::HasBindShader(RHIShaderType type) const
{
    return m_shaders.contains(type);
}

bool glTFRenderPassBase::CompileShaders()
{
    RETURN_IF_FALSE(!m_shaders.empty())

    for (const auto& shader : m_shaders)
    {
        shader.second->SetShaderCompilePreDefineMacros(m_shader_macros);
        RETURN_IF_FALSE(shader.second->CompileShader())

        // TODO: Skip raytracing shader reflection now
        if (shader.first == RHIShaderType::RayTracing)
        {
            continue;
        }
        
        RHIUtilInstanceManager::Instance().ProcessShaderMetaData(*shader.second);
    }
    
    return true;
}

RHIShaderPreDefineMacros& glTFRenderPassBase::GetShaderMacros()
{
    return m_shader_macros;
}
