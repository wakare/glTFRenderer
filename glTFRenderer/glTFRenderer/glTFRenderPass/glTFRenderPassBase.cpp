#include "glTFRenderPassBase.h"
#include "../glTFRHI/RHIResourceFactoryImpl.hpp"

glTFRenderPassBase::~glTFRenderPassBase()
= default;

bool glTFRenderPassBase::InitPass(glTFRenderResourceManager& resource_manager)
{    
    m_main_descriptor_heap = RHIResourceFactory::CreateRHIResource<IRHIDescriptorHeap>();
    RETURN_IF_FALSE(m_main_descriptor_heap->InitDescriptorHeap(resource_manager.GetDevice(),
        {static_cast<unsigned>(GetMainDescriptorHeapSize()), RHIDescriptorHeapType::CBV_SRV_UAV, true}))

    m_root_signature = RHIResourceFactory::CreateRHIResource<IRHIRootSignature>();
    
    // Init root signature
    const size_t root_signature_parameter_count = GetRootSignatureParameterCount();
    const size_t root_signature_static_sampler_count = GetRootSignatureSamplerCount();
    RETURN_IF_FALSE(m_root_signature->AllocateRootSignatureSpace(root_signature_parameter_count, root_signature_static_sampler_count))
    
    RETURN_IF_FALSE(SetupRootSignature(resource_manager))
    RETURN_IF_FALSE(m_root_signature->InitRootSignature(resource_manager.GetDevice()))
    
    return true;
}

bool glTFRenderPassBase::PreRenderPass(glTFRenderResourceManager& resource_manager)
{
    resource_manager.SetCurrentPSO(m_pipeline_state_object);
    
    auto& command_list = resource_manager.GetCommandListForRecord();
    
    const RHIViewportDesc viewport = {0, 0, (float)resource_manager.GetSwapchain().GetWidth(), (float)resource_manager.GetSwapchain().GetHeight(), 0.0f, 1.0f };
    RHIUtils::Instance().SetViewport(command_list, viewport);

    const RHIScissorRectDesc scissorRect = {0, 0, resource_manager.GetSwapchain().GetWidth(), resource_manager.GetSwapchain().GetHeight() }; 
    RHIUtils::Instance().SetScissorRect(command_list, scissorRect);
    
    RETURN_IF_FALSE(RHIUtils::Instance().SetDescriptorHeapArray(command_list, m_main_descriptor_heap.get(), 1))
    RETURN_IF_FALSE(RHIUtils::Instance().SetRootSignature(command_list, *m_root_signature, GetPipelineType() == PipelineType::Graphics))
    
    return true;
}

bool glTFRenderPassBase::RenderPass(glTFRenderResourceManager& resource_manager)
{
    return true;
}

bool glTFRenderPassBase::PostRenderPass(glTFRenderResourceManager& resource_manager)
{
    return true;
}

bool glTFRenderPassBase::SetupPipelineStateObject(glTFRenderResourceManager& resourceManager)
{
    if (m_bypass)
    {
        m_pipeline_state_object->GetShaderMacros().AddMacro("BYPASS", "1");
    }
    
    return true;
}
