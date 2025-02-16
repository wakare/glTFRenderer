#include "glTFGraphicsPassMeshOpaque.h"

#include "glTFRenderPass/glTFRenderMaterialManager.h"
#include "glTFRenderPass/glTFRenderResourceManager.h"
#include "glTFRenderPass/glTFRenderInterface/glTFRenderInterfaceRadiosityScene.h"
#include "glTFRenderPass/glTFRenderInterface/glTFRenderInterfaceSampler.h"
#include "glTFRenderPass/glTFRenderInterface/glTFRenderInterfaceSceneMaterial.h"
#include "glTFRHI/RHIUtils.h"
#include "glTFRHI/RHIResourceFactoryImpl.hpp"
#include "SceneFileLoader/glTFImageLoader.h"

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
    resource_manager.GetMemoryManager().GetDescriptorManager().CreateDescriptor(resource_manager.GetDevice(), GetResourceTexture(RenderPassResourceTableId::BasePass_Albedo), 
        RHITextureDescriptorDesc{
            GetResourceTexture(RenderPassResourceTableId::BasePass_Albedo)->GetTextureFormat(),
            RHIResourceDimension::TEXTURE2D,
            RHIViewType::RVT_RTV
        }, m_albedo_view);

    resource_manager.GetMemoryManager().GetDescriptorManager().CreateDescriptor(resource_manager.GetDevice(), GetResourceTexture(RenderPassResourceTableId::BasePass_Normal), 
        RHITextureDescriptorDesc{
            GetResourceTexture(RenderPassResourceTableId::BasePass_Normal)->GetTextureFormat(),
            RHIResourceDimension::TEXTURE2D,
            RHIViewType::RVT_RTV
        }, m_normal_view);
    
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

    std::vector<IRHITextureDescriptorAllocation*> render_targets{m_albedo_view.get(), m_normal_view.get(), &resource_manager.GetDepthDSV()};
    m_begin_rendering_info.m_render_targets = render_targets;
    m_begin_rendering_info.enable_depth_write = GetGraphicsPipelineStateObject().GetDepthStencilMode() == RHIDepthStencilMode::DEPTH_WRITE;
    m_begin_rendering_info.clear_render_target = true;
    
    return true;
}

bool glTFGraphicsPassMeshOpaque::ModifyFinalOutput(RenderGraphNodeUtil::RenderGraphNodeFinalOutput& final_output)
{
    final_output.final_color_output = GetResourceTexture(RenderPassResourceTableId::BasePass_Albedo);
    
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
    
    GetGraphicsPipelineStateObject().BindRenderTargetFormats({m_albedo_view.get(), m_normal_view.get(),
        &resource_manager.GetDepthDSV()});
    
    //GetGraphicsPipelineStateObject().SetDepthStencilState(RHIDepthStencilMode::DEPTH_WRITE);
    
    return true;
}

bool glTFGraphicsPassMeshOpaque::InitResourceTable(glTFRenderResourceManager& resource_manager)
{
    RETURN_IF_FALSE(glTFGraphicsPassMeshBase::InitResourceTable(resource_manager))

    AddExportTextureResource(RHITextureDesc::MakeBasePassAlbedoTextureDesc(resource_manager), RenderPassResourceTableId::BasePass_Albedo);
    AddExportTextureResource(RHITextureDesc::MakeBasePassNormalTextureDesc(resource_manager), RenderPassResourceTableId::BasePass_Normal);
    
    return true;
}
