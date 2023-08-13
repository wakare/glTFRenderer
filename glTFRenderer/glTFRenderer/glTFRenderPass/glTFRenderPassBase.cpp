#include "glTFRenderPassBase.h"
#include "../glTFRHI/RHIResourceFactoryImpl.hpp"

glTFRenderPassBase::~glTFRenderPassBase()
= default;

bool glTFRenderPassBase::ProcessMaterial(glTFRenderResourceManager& resourceManager, const glTFMaterialBase& material)
{
    return false;
}

bool glTFRenderPassBase::InitPass(glTFRenderResourceManager& resourceManager)
{    
    m_main_descriptor_heap = RHIResourceFactory::CreateRHIResource<IRHIDescriptorHeap>();
    RETURN_IF_FALSE(m_main_descriptor_heap->InitDescriptorHeap(resourceManager.GetDevice(),
        {static_cast<unsigned>(GetMainDescriptorHeapSize()), RHIDescriptorHeapType::CBV_SRV_UAV, true}))

    m_root_signature = RHIResourceFactory::CreateRHIResource<IRHIRootSignature>();
    RETURN_IF_FALSE(SetupRootSignature(resourceManager))
    
    m_pipeline_state_object = RHIResourceFactory::CreateRHIResource<IRHIPipelineStateObject>();
    
    // glTF using CCW as front face
    m_pipeline_state_object->SetCullMode(IRHICullMode::CW);
    RETURN_IF_FALSE(SetupPipelineStateObject(resourceManager))
    
    return true;
}

bool glTFRenderPassBase::RenderPass(glTFRenderResourceManager& resourceManager)
{
    resourceManager.SetCurrentPSO(m_pipeline_state_object);
    
    const RHIViewportDesc viewport = {0, 0, (float)resourceManager.GetSwapchain().GetWidth(), (float)resourceManager.GetSwapchain().GetHeight(), 0.0f, 1.0f };
    RHIUtils::Instance().SetViewport(resourceManager.GetCommandListForRecord(), viewport);

    const RHIScissorRectDesc scissorRect = {0, 0, resourceManager.GetSwapchain().GetWidth(), resourceManager.GetSwapchain().GetHeight() }; 
    RHIUtils::Instance().SetScissorRect(resourceManager.GetCommandListForRecord(), scissorRect);
    
    RETURN_IF_FALSE(RHIUtils::Instance().SetDescriptorHeapArray(resourceManager.GetCommandListForRecord(), m_main_descriptor_heap.get(), 1))
    RETURN_IF_FALSE(RHIUtils::Instance().SetRootSignature(resourceManager.GetCommandListForRecord(), *m_root_signature))
    
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
