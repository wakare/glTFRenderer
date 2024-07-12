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
    : m_material_uploaded(false)
{
    AddRenderInterface(std::make_shared<glTFRenderInterfaceRadiosityScene>());
    AddRenderInterface(std::make_shared<glTFRenderInterfaceSceneMaterial>());
    
    const std::shared_ptr<glTFRenderInterfaceSampler<RHIStaticSamplerAddressMode::Warp, RHIStaticSamplerFilterMode::Linear>> sampler_interface =
        std::make_shared<glTFRenderInterfaceSampler<RHIStaticSamplerAddressMode::Warp, RHIStaticSamplerFilterMode::Linear>>();
    sampler_interface->SetSamplerRegisterIndexName("DEFAULT_SAMPLER_REGISTER_INDEX");
    AddRenderInterface(sampler_interface);
}

bool glTFGraphicsPassMeshOpaque::InitPass(glTFRenderResourceManager& resource_manager)
{
    resource_manager.GetMemoryManager().GetDescriptorManager().CreateDescriptor(resource_manager.GetDevice(), GetResourceTexture(RenderPassResourceTableId::BasePass_Albedo), 
        {
            .format = GetResourceTexture(RenderPassResourceTableId::BasePass_Albedo).GetTextureFormat(),
            .dimension = RHIResourceDimension::TEXTURE2D,
            .view_type = RHIViewType::RVT_RTV
        }, m_albedo_view);

    resource_manager.GetMemoryManager().GetDescriptorManager().CreateDescriptor(resource_manager.GetDevice(), GetResourceTexture(RenderPassResourceTableId::BasePass_Normal), 
        {
            .format = GetResourceTexture(RenderPassResourceTableId::BasePass_Normal).GetTextureFormat(),
            .dimension = RHIResourceDimension::TEXTURE2D,
            .view_type = RHIViewType::RVT_RTV
        }, m_normal_view);
    
    RETURN_IF_FALSE(glTFGraphicsPassMeshBase::InitPass(resource_manager))

    if (!m_material_uploaded)
    {
        GetRenderInterface<glTFRenderInterfaceSceneMaterial>()->UploadMaterialData(resource_manager);
        m_material_uploaded = true;
    }
    
    return true;
}

bool glTFGraphicsPassMeshOpaque::PreRenderPass(glTFRenderResourceManager& resource_manager)
{
    RETURN_IF_FALSE(glTFGraphicsPassMeshBase::PreRenderPass(resource_manager))

    auto& command_list = resource_manager.GetCommandListForRecord();
    
    // Transition swapchain state to render target for shading
    resource_manager.GetCurrentFrameSwapChainRT().Transition(command_list, RHIResourceStateType::STATE_RENDER_TARGET);
    GetResourceTexture(RenderPassResourceTableId::BasePass_Albedo).Transition(command_list, RHIResourceStateType::STATE_RENDER_TARGET);
    GetResourceTexture(RenderPassResourceTableId::BasePass_Normal).Transition(command_list, RHIResourceStateType::STATE_RENDER_TARGET);
    
    RETURN_IF_FALSE(resource_manager.GetRenderTargetManager().BindRenderTarget(command_list,
            {m_albedo_view.get(), m_normal_view.get()}, &resource_manager.GetDepthRT().GetDescriptorAllocation()))

    RHITextureClearValue render_target_clear_value = GetResourceTexture(RenderPassResourceTableId::BasePass_Albedo).GetTextureDesc().GetClearValue();
    RHITextureClearValue depth_stencil_clear_value = resource_manager.GetDepthRT().GetClearValue();
     
    RETURN_IF_FALSE(resource_manager.GetRenderTargetManager().ClearRenderTarget(command_list,
            {m_albedo_view.get(), m_normal_view.get()}, render_target_clear_value, depth_stencil_clear_value))

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
    
    GetResourceTexture(RenderPassResourceTableId::BasePass_Albedo).Transition(command_list, RHIResourceStateType::STATE_RENDER_TARGET);
    GetResourceTexture(RenderPassResourceTableId::BasePass_Normal).Transition(command_list, RHIResourceStateType::STATE_RENDER_TARGET);
    
    GetGraphicsPipelineStateObject().BindRenderTargetFormats({m_albedo_view.get(), m_normal_view.get(), &resource_manager.GetDepthRT().GetDescriptorAllocation()});
    
    return true;
}

bool glTFGraphicsPassMeshOpaque::InitResourceTable(glTFRenderResourceManager& resource_manager)
{
    RETURN_IF_FALSE(glTFGraphicsPassMeshBase::InitResourceTable(resource_manager))

    AddExportTextureResource(RHITextureDesc::MakeBasePassAlbedoTextureDesc(resource_manager), RenderPassResourceTableId::BasePass_Albedo);
    AddExportTextureResource(RHITextureDesc::MakeBasePassNormalTextureDesc(resource_manager), RenderPassResourceTableId::BasePass_Normal);
    
    return true;
}
