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
    RETURN_IF_FALSE (m_pipeline_state_object->InitGraphicsPipelineStateObject(resource_manager.GetDevice(), *m_root_signature, resource_manager.GetSwapchain()))
    
    return true;
}

bool glTFGraphicsPassBase::ProcessMaterial(glTFRenderResourceManager& resourceManager, const glTFMaterialBase& material)
{
    // default do no processing for material
    return true;
}

std::shared_ptr<IRHIPipelineStateObject> glTFGraphicsPassBase::GetPSO() const
{
    GLTF_CHECK(m_pipeline_state_object);
    return m_pipeline_state_object;
}

bool glTFGraphicsPassBase::SetupPipelineStateObject(glTFRenderResourceManager& resourceManager)
{
    RETURN_IF_FALSE(glTFRenderPassBase::SetupPipelineStateObject(resourceManager))

    // Set shader macro based vertex attributes
    RETURN_IF_FALSE(m_pipeline_state_object->BindInputLayoutAndSetShaderMacros(GetVertexInputLayout()))

    return true;
}
