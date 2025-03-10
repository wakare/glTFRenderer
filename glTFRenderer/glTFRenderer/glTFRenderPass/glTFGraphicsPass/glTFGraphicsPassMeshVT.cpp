#include "glTFGraphicsPassMeshVT.h"

#include "glTFRenderPass/glTFRenderResourceManager.h"
#include "glTFRenderPass/glTFRenderInterface/glTFRenderInterfaceSampler.h"
#include "glTFRenderPass/glTFRenderInterface/glTFRenderInterfaceSceneMaterial.h"
#include "glTFRHI/RHIInterface/IRHIPipelineStateObject.h"

bool glTFGraphicsPassMeshVT::InitRenderInterface(glTFRenderResourceManager& resource_manager)
{
    RETURN_IF_FALSE(glTFGraphicsPassMeshBase::InitRenderInterface(resource_manager))

    AddRenderInterface(std::make_shared<glTFRenderInterfaceSceneMaterial>());
    AddRenderInterface(std::make_shared<glTFRenderInterfaceSampler<RHIStaticSamplerAddressMode::Warp, RHIStaticSamplerFilterMode::Linear>>("DEFAULT_SAMPLER_REGISTER_INDEX"));
    
    return true;
}

bool glTFGraphicsPassMeshVT::SetupPipelineStateObject(glTFRenderResourceManager& resource_manager)
{
    RETURN_IF_FALSE(glTFGraphicsPassMeshBase::SetupPipelineStateObject(resource_manager))

    GetGraphicsPipelineStateObject().BindShaderCode(
        R"(glTFResources\ShaderSource\MeshPassCommonVS.hlsl)", RHIShaderType::Vertex, "main");
    GetGraphicsPipelineStateObject().BindShaderCode(
        R"(glTFResources\ShaderSource\MeshPassVTFeedBackPS.hlsl)", RHIShaderType::Pixel, "main");
    auto& command_list = resource_manager.GetCommandListForRecord();
    
    GetResourceTexture(RenderPassResourceTableId::BasePass_VT_Feedback)->Transition(command_list, RHIResourceStateType::STATE_RENDER_TARGET);
    
    GetGraphicsPipelineStateObject().BindRenderTargetFormats(
        {
            GetResourceDescriptor(RenderPassResourceTableId::BasePass_VT_Feedback).get(),
        });
    
    return true;
}

bool glTFGraphicsPassMeshVT::PreRenderPass(glTFRenderResourceManager& resource_manager)
{
    RETURN_IF_FALSE(glTFGraphicsPassMeshBase::PreRenderPass(resource_manager))

    auto& command_list = resource_manager.GetCommandListForRecord();
    GetResourceTexture(RenderPassResourceTableId::BasePass_VT_Feedback)->Transition(command_list, RHIResourceStateType::STATE_RENDER_TARGET);

    m_begin_rendering_info.m_render_targets = {GetResourceDescriptor(RenderPassResourceTableId::BasePass_VT_Feedback).get()};
    m_begin_rendering_info.enable_depth_write = GetGraphicsPipelineStateObject().GetDepthStencilMode() == RHIDepthStencilMode::DEPTH_WRITE;
    m_begin_rendering_info.clear_render_target = true;
    
    return true;
}

bool glTFGraphicsPassMeshVT::InitResourceTable(glTFRenderResourceManager& resource_manager)
{
    RETURN_IF_FALSE(glTFGraphicsPassMeshBase::InitResourceTable(resource_manager))
    
    auto feedback_desc = RHITextureDesc::MakeBasePassVTFeedbackDesc(resource_manager);
    AddExportTextureResource(RenderPassResourceTableId::BasePass_VT_Feedback, feedback_desc, 
    {
        feedback_desc.GetDataFormat(),
        RHIResourceDimension::TEXTURE2D,
        RHIViewType::RVT_RTV
    });

    AddFinalOutputCandidate(RenderPassResourceTableId::BasePass_VT_Feedback);
    
    return true;
}
