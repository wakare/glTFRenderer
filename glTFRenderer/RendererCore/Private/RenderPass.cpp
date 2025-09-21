#include "RenderPass.h"

#include "InternalResourceHandleTable.h"
#include "ResourceManager.h"
#include "RHIResourceFactoryImpl.hpp"
#include "RHIInterface/IRHIPipelineStateObject.h"

RenderPass::RenderPass(RendererInterface::RenderPassDesc desc)
    : m_desc(std::move(desc))
{
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
    std::vector<ShaderMetaDataDSParameter> shader_meta_data_resource_parameters;
    std::vector<ShaderMetaDataDSParameter> shader_meta_data_samplers;

    std::map<RHIShaderType, std::shared_ptr<IRHIShader>> shaders;
    for (const auto& shader_pair : m_desc.shaders)
    {
        auto shader = RendererInterface::InternalResourceHandleTable::Instance().GetShader(shader_pair.second);
        auto& shader_meta_data = shader->GetMetaData();
        for (const auto& root_parameter_info : shader_meta_data.root_parameter_infos)
        {
            RootSignatureAllocation allocation;
            m_root_signature_helper.AddRootParameterWithRegisterCount(root_parameter_info, allocation);

            m_shader_parameter_mapping.insert({root_parameter_info.parameter_name, allocation});
        }

        shaders[shader->GetType()] = shader;
    }
    
    // Init root signature
    m_root_signature_helper.BuildRootSignature(resource_manager.GetDevice(), resource_manager.GetMemoryManager().GetDescriptorManager());
    auto* m_root_signature = &m_root_signature_helper.GetRootSignature(); 
    
    unsigned resource_index = 0;
    for (const auto& resource_parameter : shader_meta_data_resource_parameters)
    {
        auto& root_parameter = m_root_signature->GetRootParameter(resource_index++);
        switch (resource_parameter.descriptor_type) {
        case ShaderMetaDataDSType::Constant:
            root_parameter.InitAsConstant(0, resource_parameter.binding_index, resource_parameter.space_index);
            break;
        case ShaderMetaDataDSType::CBV:
            // Attribute index is no need in DX12
            root_parameter.InitAsCBV(0, resource_parameter.binding_index, resource_parameter.space_index);
            break;
        case ShaderMetaDataDSType::SRV:
            root_parameter.InitAsSRV(0, resource_parameter.binding_index, resource_parameter.space_index);
            break;
        case ShaderMetaDataDSType::UAV:
            root_parameter.InitAsUAV(0, resource_parameter.binding_index, resource_parameter.space_index);
            break;
        }
    }

    unsigned sampler_index = 0;
    for (const auto& sampler : shader_meta_data_samplers)
    {
        auto& root_sampler = m_root_signature->GetStaticSampler(sampler_index++);
        // TODO: Init static samplers by external desc
        root_sampler.InitStaticSampler(resource_manager.GetDevice(), sampler.space_index, sampler.binding_index, RHIStaticSamplerAddressMode::Border, RHIStaticSamplerFilterMode::Linear);
    }

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
