#include "glTFGraphicsPassBase.h"
#include "../glTFRHI/RHIResourceFactoryImpl.hpp"

glTFGraphicsPassBase::glTFGraphicsPassBase()
= default;

bool glTFGraphicsPassBase::InitPass(glTFRenderResourceManager& resource_manager)
{
    RETURN_IF_FALSE(glTFRenderPassBase::InitPass(resource_manager))

    m_pipeline_state_object = RHIResourceFactory::CreateRHIResource<IRHIGraphicsPipelineStateObject>();
    // glTF using CCW as front face
    m_pipeline_state_object->SetCullMode(IRHICullMode::CW);

    RETURN_IF_FALSE(SetupPipelineStateObject(resource_manager))
    RETURN_IF_FALSE (GetGraphicsPipelineStateObject().InitGraphicsPipelineStateObject(resource_manager.GetDevice(), *m_root_signature, resource_manager.GetSwapchain()))
    
    return true;
}

IRHIGraphicsPipelineStateObject& glTFGraphicsPassBase::GetGraphicsPipelineStateObject() const
{
    return dynamic_cast<IRHIGraphicsPipelineStateObject&>(*m_pipeline_state_object);
}

bool glTFGraphicsPassBase::SetupPipelineStateObject(glTFRenderResourceManager& resource_manager)
{
    RETURN_IF_FALSE(glTFRenderPassBase::SetupPipelineStateObject(resource_manager))

    // Set shader macro based vertex attributes
    RETURN_IF_FALSE(GetGraphicsPipelineStateObject().BindInputLayoutAndSetShaderMacros(GetVertexInputLayout()))

    return true;
}
