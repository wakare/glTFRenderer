#include "glTFRenderPassBase.h"
#include "../glTFRHI/RHIResourceFactoryImpl.hpp"

glTFRenderPassBase::~glTFRenderPassBase()
= default;

bool glTFRenderPassBase::ProcessMaterial(glTFRenderResourceManager& resourceManager, const glTFMaterialBase& material)
{
    // default do no processing for material
    return true;
}

bool glTFRenderPassBase::InitPass(glTFRenderResourceManager& resource_manager)
{    
    m_main_descriptor_heap = RHIResourceFactory::CreateRHIResource<IRHIDescriptorHeap>();
    RETURN_IF_FALSE(m_main_descriptor_heap->InitDescriptorHeap(resource_manager.GetDevice(),
        {static_cast<unsigned>(GetMainDescriptorHeapSize()), RHIDescriptorHeapType::CBV_SRV_UAV, true}))

    m_root_signature = RHIResourceFactory::CreateRHIResource<IRHIRootSignature>();
    RETURN_IF_FALSE(SetupRootSignature(resource_manager))
    
    m_pipeline_state_object = RHIResourceFactory::CreateRHIResource<IRHIGraphicsPipelineStateObject>();
    // glTF using CCW as front face
    m_pipeline_state_object->SetCullMode(IRHICullMode::CW);
    
    RETURN_IF_FALSE(SetupPipelineStateObject(resource_manager))
    RETURN_IF_FALSE (m_pipeline_state_object->InitGraphicsPipelineStateObject(resource_manager.GetDevice(), *m_root_signature, resource_manager.GetSwapchain()))
    
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
    RETURN_IF_FALSE(RHIUtils::Instance().SetRootSignature(command_list, *m_root_signature))
    
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

IRHIPipelineStateObject& glTFRenderPassBase::GetPSO() const
{
    return *m_pipeline_state_object;
}

bool glTFRenderPassBase::SetupPipelineStateObject(glTFRenderResourceManager& resourceManager)
{
    // Set shader macro based vertex attributes
    RETURN_IF_FALSE(m_pipeline_state_object->BindInputLayoutAndSetShaderMacros(GetVertexInputLayout()))

    if (m_bypass)
    {
        m_pipeline_state_object->GetShaderMacros().AddMacro("BYPASS", "1");
    }
    
    return true;
}
