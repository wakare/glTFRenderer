#include "glTFGraphicsPassMeshOpaque.h"

#include "glTFRenderPass/glTFRenderMaterialManager.h"
#include "glTFRenderPass/glTFRenderResourceManager.h"
#include "glTFRenderPass/glTFRenderInterface/glTFRenderInterfaceRadiosityScene.h"
#include "glTFRenderPass/glTFRenderInterface/glTFRenderInterfaceSampler.h"
#include "glTFRenderPass/glTFRenderInterface/glTFRenderInterfaceSceneMaterial.h"
#include "glTFRHI/RHIUtils.h"
#include "glTFRHI/RHIResourceFactoryImpl.hpp"

glTFGraphicsPassMeshOpaque::glTFGraphicsPassMeshOpaque()
{
    
}

bool glTFGraphicsPassMeshOpaque::InitRenderInterface(glTFRenderResourceManager& resource_manager)
{
    RETURN_IF_FALSE(glTFGraphicsPassMeshBase::InitRenderInterface(resource_manager))

    AddRenderInterface(std::make_shared<glTFRenderInterfaceRadiosityScene>());
    AddRenderInterface(std::make_shared<glTFRenderInterfaceSceneMaterial>());
    
    const std::shared_ptr<glTFRenderInterfaceSampler<RHIStaticSamplerAddressMode::Warp, RHIStaticSamplerFilterMode::Linear>> sampler_interface =
        std::make_shared<glTFRenderInterfaceSampler<RHIStaticSamplerAddressMode::Warp, RHIStaticSamplerFilterMode::Linear>>("DEFAULT_SAMPLER_REGISTER_INDEX");
    AddRenderInterface(sampler_interface);
    
    return true;
}

bool glTFGraphicsPassMeshOpaque::InitPass(glTFRenderResourceManager& resource_manager)
{
    RETURN_IF_FALSE(glTFGraphicsPassMeshBase::InitPass(resource_manager))
    
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

    std::vector<IRHITextureDescriptorAllocation*> render_targets
        {
            GetResourceDescriptor(RenderPassResourceTableId::BasePass_Albedo).get(),
            GetResourceDescriptor(RenderPassResourceTableId::BasePass_Normal).get(),
            &resource_manager.GetDepthDSV()
        };
    
    m_begin_rendering_info.m_render_targets = render_targets;
    m_begin_rendering_info.enable_depth_write = GetGraphicsPipelineStateObject().GetDepthStencilMode() == RHIDepthStencilMode::DEPTH_WRITE;
    m_begin_rendering_info.clear_render_target = true;
    
    return true;
}

bool glTFGraphicsPassMeshOpaque::SetupRootSignature(glTFRenderResourceManager& resource_manager)
{
    RETURN_IF_FALSE(glTFGraphicsPassMeshBase::SetupRootSignature(resource_manager))
    
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
            GetResourceDescriptor(RenderPassResourceTableId::BasePass_Albedo).get(),
            &resource_manager.GetDepthDSV()
        });
    
    return true;
}

bool glTFGraphicsPassMeshOpaque::InitResourceTable(glTFRenderResourceManager& resource_manager)
{
    RETURN_IF_FALSE(glTFGraphicsPassMeshBase::InitResourceTable(resource_manager))

    auto albedo_desc = RHITextureDesc::MakeBasePassAlbedoTextureDesc(resource_manager); 
    AddExportTextureResource(RenderPassResourceTableId::BasePass_Albedo, albedo_desc, 
    {
        albedo_desc.GetDataFormat(),
        RHIResourceDimension::TEXTURE2D,
        RHIViewType::RVT_RTV
    });

    auto normal_desc = RHITextureDesc::MakeBasePassNormalTextureDesc(resource_manager);
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
