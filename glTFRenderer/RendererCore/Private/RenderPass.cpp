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
    
    for (const auto& shader_pair : m_desc.shaders)
    {
        auto shader = RendererInterface::InternalResourceHandleTable::Instance().GetShader(shader_pair.second);
        auto& shader_meta_data = shader->GetMetaData();
        for (const auto& shader_parameter : shader_meta_data.parameter_infos)
        {
            if (shader_parameter.descriptor_type != ShaderMetaDataDSType::Sampler)
            {
                shader_meta_data_resource_parameters.push_back(shader_parameter);
            }
            else
            {
                shader_meta_data_samplers.push_back(shader_parameter);
            }
        }
    }
    
    // Init root signature
    m_root_signature = RHIResourceFactory::CreateRHIResource<IRHIRootSignature>();
    m_root_signature->AllocateRootSignatureSpace(shader_meta_data_resource_parameters.size(), shader_meta_data_samplers.size());
    m_root_signature->SetUsage(RHIRootSignatureUsage::Default);

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
    
    RETURN_IF_FALSE(m_pipeline_state_object->InitPipelineStateObject(resource_manager.GetDevice(),
            *m_root_signature,
            resource_manager.GetSwapChain()
        ))
    
    return true;
}
