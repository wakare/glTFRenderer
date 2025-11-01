#include "RenderPass.h"

#include "InternalResourceHandleTable.h"
#include "ResourceManager.h"
#include "RHIResourceFactoryImpl.hpp"
#include "RHIInterface/IRHIPipelineStateObject.h"

RenderPass::RenderPass(RendererInterface::RenderPassDesc desc)
    : m_desc(std::move(desc))
{
    m_viewport_width = m_desc.viewport_width;
    m_viewport_height = m_desc.viewport_height;
}

bool RenderPass::InitRenderPass(ResourceManager& resource_manager)
{
    switch (m_desc.type) {
    case RendererInterface::RenderPassType::GRAPHICS:
        m_pipeline_state_object = RHIResourceFactory::CreateRHIResource<IRHIGraphicsPipelineStateObject>();
        break;
    case RendererInterface::RenderPassType::COMPUTE:
        m_pipeline_state_object = RHIResourceFactory::CreateRHIResource<IRHIComputePipelineStateObject>();
        break;
    case RendererInterface::RenderPassType::RAY_TRACING:
        m_pipeline_state_object = RHIResourceFactory::CreateRHIResource<IRHIRayTracingPipelineStateObject>();
        break;
    }

    // Build root signature from shader reflection
    std::map<RHIShaderType, std::shared_ptr<IRHIShader>> shaders;
    for (const auto& shader_pair : m_desc.shaders)
    {
        auto shader = RendererInterface::InternalResourceHandleTable::Instance().GetShader(shader_pair.second);
        auto& shader_meta_data = shader->GetMetaData();
        for (const auto& root_parameter_info : shader_meta_data.root_parameter_infos)
        {
            RootSignatureAllocation allocation;
            m_root_signature_helper.AddRootParameterWithRegisterCount2(root_parameter_info.parameter_info, root_parameter_info.space, root_parameter_info.register_index, allocation);

            m_shader_parameter_mapping.insert({root_parameter_info.parameter_info.parameter_name, allocation});
        }

        shaders[shader->GetType()] = shader;
    }
    
    // Init root signature
    m_root_signature_helper.BuildRootSignature(resource_manager.GetDevice(), resource_manager.GetMemoryManager().GetDescriptorManager());
    auto* m_root_signature = &m_root_signature_helper.GetRootSignature(); 
    
    m_root_signature->InitRootSignature(resource_manager.GetDevice(), resource_manager.GetMemoryManager().GetDescriptorManager());

    if (m_pipeline_state_object->GetPSOType() == RHIPipelineType::Graphics)
    {
        auto& graphics_pipeline_state_object = dynamic_cast<IRHIGraphicsPipelineStateObject&>(*m_pipeline_state_object);
        std::vector<RHIDataFormat> render_target_formats;
        render_target_formats.reserve(m_desc.render_target_bindings.size());
        for (const auto& render_target : m_desc.render_target_bindings)
        {
            render_target_formats.push_back(RendererInterfaceRHIConverter::ConvertToRHIFormat(render_target.format));
        }
        
        graphics_pipeline_state_object.BindRenderTargetFormats(render_target_formats);
    }

    m_pipeline_state_object->SetCullMode(RHICullMode::NONE);
    m_pipeline_state_object->SetDepthStencilState(RHIDepthStencilMode::DEPTH_WRITE);
    
    RETURN_IF_FALSE(m_pipeline_state_object->InitPipelineStateObject(resource_manager.GetDevice(),
            *m_root_signature,
            resource_manager.GetSwapChain(),
            shaders
        ))

    m_descriptor_updater = RHIResourceFactory::CreateRHIResource<IRHIDescriptorUpdater>();

    return true;
}

IRHIPipelineStateObject& RenderPass::GetPipelineStateObject()
{
    return *m_pipeline_state_object;
}

IRHIRootSignature& RenderPass::GetRootSignature()
{
    return m_root_signature_helper.GetRootSignature();
}

RendererInterface::RenderPassType RenderPass::GetRenderPassType() const
{
    return m_desc.type;
}

const RootSignatureAllocation& RenderPass::GetRootSignatureAllocation(const std::string& name) const
{
    GLTF_CHECK(m_shader_parameter_mapping.contains(name));

    return m_shader_parameter_mapping.at(name);
}

IRHIDescriptorUpdater& RenderPass::GetDescriptorUpdater()
{
    return *m_descriptor_updater;
}

std::pair<int, int> RenderPass::GetViewportSize() const
{
    return {m_viewport_width, m_viewport_height};
}

void RenderPass::SetViewportSize(int width, int height)
{
    m_viewport_width = width;
    m_viewport_height = height;
}
