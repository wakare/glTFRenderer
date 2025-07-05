#include "glTFGraphicsPassMeshOpaque.h"

#include "glTFRenderPass/glTFRenderMaterialManager.h"
#include "glTFRenderPass/glTFRenderResourceManager.h"
#include "glTFRenderPass/glTFRenderInterface/glTFRenderInterfaceSampler.h"
#include "glTFRenderPass/glTFRenderInterface/glTFRenderInterfaceSceneMaterial.h"
#include "glTFRenderPass/glTFRenderInterface/glTFRenderInterfaceVT.h"
#include "glTFRenderSystem/VT/VirtualTextureSystem.h"
#include "RHIResourceFactoryImpl.hpp"

bool glTFGraphicsPassMeshOpaque::InitRenderInterface(glTFRenderResourceManager& resource_manager)
{
    RETURN_IF_FALSE(glTFGraphicsPassMeshBaseSceneView::InitRenderInterface(resource_manager))

    AddRenderInterface(std::make_shared<glTFRenderInterfaceSceneMaterial>());

    if (resource_manager.GetRenderSystem<VirtualTextureSystem>())
    {
        AddRenderInterface(std::make_shared<glTFRenderInterfaceVT>(InterfaceVTType::SAMPLE_VT_TEXTURE_DATA));    
    }
    
    AddRenderInterface(std::make_shared<glTFRenderInterfaceSampler<RHIStaticSamplerAddressMode::Warp, RHIStaticSamplerFilterMode::Linear>>("DEFAULT_SAMPLER_REGISTER_INDEX"));
    
    return true;
}

bool glTFGraphicsPassMeshOpaque::PreRenderPass(glTFRenderResourceManager& resource_manager)
{
    RETURN_IF_FALSE(glTFGraphicsPassMeshBase::PreRenderPass(resource_manager))

    auto& command_list = resource_manager.GetCommandListForRecord();
    
    // Transition swapchain state to render target for shading
    resource_manager.GetCurrentFrameSwapChainTexture().Transition(command_list, RHIResourceStateType::STATE_RENDER_TARGET);
    GetResourceTexture(RenderPassResourceTableId::BasePass_Albedo)->Transition(command_list, RHIResourceStateType::STATE_RENDER_TARGET);
    GetResourceTexture(RenderPassResourceTableId::BasePass_Normal)->Transition(command_list, RHIResourceStateType::STATE_RENDER_TARGET);

    std::vector render_targets
        {
            GetResourceDescriptor(RenderPassResourceTableId::BasePass_Albedo).get(),
            GetResourceDescriptor(RenderPassResourceTableId::BasePass_Normal).get(),
            &resource_manager.GetDepthDSV()
        };
    
    m_begin_rendering_info.m_render_targets = render_targets;
    m_begin_rendering_info.enable_depth_write = GetGraphicsPipelineStateObject().GetDepthStencilMode() == RHIDepthStencilMode::DEPTH_WRITE;
    m_begin_rendering_info.clear_render_target = true;
    
    // Has prepass depth buffer, do not clear
    m_begin_rendering_info.clear_depth_stencil = false;
    
    return true;
}

bool glTFGraphicsPassMeshOpaque::SetupPipelineStateObject(glTFRenderResourceManager& resource_manager)
{
    RETURN_IF_FALSE(glTFGraphicsPassMeshBase::SetupPipelineStateObject(resource_manager))
    
    GetGraphicsPipelineStateObject().BindShaderCode(
        R"(glTFResources\ShaderSource\MeshPassCommonVS.hlsl)", RHIShaderType::Vertex, "main");
    GetGraphicsPipelineStateObject().BindShaderCode(
        R"(glTFResources\ShaderSource\MeshPassCommonPS.hlsl)", RHIShaderType::Pixel, "main");
    auto& command_list = resource_manager.GetCommandListForRecord();
    
    GetResourceTexture(RenderPassResourceTableId::BasePass_Albedo)->Transition(command_list, RHIResourceStateType::STATE_RENDER_TARGET);
    GetResourceTexture(RenderPassResourceTableId::BasePass_Normal)->Transition(command_list, RHIResourceStateType::STATE_RENDER_TARGET);
    
    GetGraphicsPipelineStateObject().BindRenderTargetFormats(
        {
            GetResourceDescriptor(RenderPassResourceTableId::BasePass_Albedo).get(),
            GetResourceDescriptor(RenderPassResourceTableId::BasePass_Normal).get(),
            &resource_manager.GetDepthDSV()
        });
    
    return true;
}

bool glTFGraphicsPassMeshOpaque::InitResourceTable(glTFRenderResourceManager& resource_manager)
{
    RETURN_IF_FALSE(glTFGraphicsPassMeshBase::InitResourceTable(resource_manager))

    const unsigned width = resource_manager.GetSwapChain().GetWidth();
    const unsigned height = resource_manager.GetSwapChain().GetHeight();
    
    auto albedo_desc = RHITextureDesc::MakeBasePassAlbedoTextureDesc(width, height); 
    AddExportTextureResource(RenderPassResourceTableId::BasePass_Albedo, albedo_desc, 
    {
        albedo_desc.GetDataFormat(),
        RHIResourceDimension::TEXTURE2D,
        RHIViewType::RVT_RTV
    });

    auto normal_desc = RHITextureDesc::MakeBasePassNormalTextureDesc(width, height);
    AddExportTextureResource(RenderPassResourceTableId::BasePass_Normal, normal_desc, 
    {
        normal_desc.GetDataFormat(),
        RHIResourceDimension::TEXTURE2D,
        RHIViewType::RVT_RTV
    });

    AddFinalOutputCandidate(RenderPassResourceTableId::BasePass_Albedo);
    AddFinalOutputCandidate(RenderPassResourceTableId::BasePass_Normal);
    
    return true;
}
