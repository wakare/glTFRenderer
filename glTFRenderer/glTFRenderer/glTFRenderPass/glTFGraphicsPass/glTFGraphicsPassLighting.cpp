#include "glTFGraphicsPassLighting.h"
#include "glTFRHI/RHIInterface/IRHIPipelineStateObject.h"
#include "glTFLight/glTFLightBase.h"
#include "glTFRenderPass/glTFRenderResourceManager.h"
#include "glTFRenderPass/glTFRenderInterface/glTFRenderInterfaceSampler.h"
#include "glTFRenderPass/glTFRenderInterface/glTFRenderInterfaceSceneView.h"

glTFGraphicsPassLighting::glTFGraphicsPassLighting()
{
}

const char* glTFGraphicsPassLighting::PassName()
{
    return "GraphicsPassLighting";
}

bool glTFGraphicsPassLighting::InitRenderInterface(glTFRenderResourceManager& resource_manager)
{
    RETURN_IF_FALSE(glTFGraphicsPassPostprocess::InitRenderInterface(resource_manager))

    AddRenderInterface(std::make_shared<glTFRenderInterfaceSceneView>());
    AddRenderInterface(std::make_shared<glTFRenderInterfaceLighting>());
    const std::shared_ptr<glTFRenderInterfaceSampler<RHIStaticSamplerAddressMode::Clamp, RHIStaticSamplerFilterMode::Linear>> sampler_interface =
        std::make_shared<glTFRenderInterfaceSampler<RHIStaticSamplerAddressMode::Clamp, RHIStaticSamplerFilterMode::Linear>>("DEFAULT_SAMPLER_REGISTER_INDEX");
    AddRenderInterface(sampler_interface);
    
    return true;
}

bool glTFGraphicsPassLighting::InitPass(glTFRenderResourceManager& resource_manager)
{
    RETURN_IF_FALSE(glTFGraphicsPassPostprocess::InitPass(resource_manager))

    return true;
}

bool glTFGraphicsPassLighting::PreRenderPass(glTFRenderResourceManager& resource_manager)
{
    RETURN_IF_FALSE(glTFGraphicsPassPostprocess::PreRenderPass(resource_manager))

    auto& command_list = resource_manager.GetCommandListForRecord();
    
    GetResourceTexture(RenderPassResourceTableId::BasePass_Albedo)->Transition(command_list, RHIResourceStateType::STATE_PIXEL_SHADER_RESOURCE);
    GetResourceTexture(RenderPassResourceTableId::BasePass_Normal)->Transition(command_list, RHIResourceStateType::STATE_PIXEL_SHADER_RESOURCE);
    GetResourceTexture(RenderPassResourceTableId::Depth)->Transition(command_list, RHIResourceStateType::STATE_PIXEL_SHADER_RESOURCE);

    BindDescriptor(command_list, m_albedo_allocation, *GetResourceDescriptor(RenderPassResourceTableId::BasePass_Albedo));
    BindDescriptor(command_list, m_depth_allocation, *GetResourceDescriptor(RenderPassResourceTableId::Depth));
    BindDescriptor(command_list, m_normal_allocation, *GetResourceDescriptor(RenderPassResourceTableId::BasePass_Normal));

    RETURN_IF_FALSE(GetRenderInterface<glTFRenderInterfaceLighting>()->UpdateCPUBuffer(resource_manager))

    m_begin_rendering_info.m_render_targets = {&resource_manager.GetCurrentFrameSwapChainRTV()};
    m_begin_rendering_info.enable_depth_write = GetGraphicsPipelineStateObject().GetDepthStencilMode() == RHIDepthStencilMode::DEPTH_WRITE;;
    
    return true;
}

bool glTFGraphicsPassLighting::PostRenderPass(glTFRenderResourceManager& resource_manager)
{
    RETURN_IF_FALSE(glTFGraphicsPassPostprocess::PostRenderPass(resource_manager))

    return true;
}

bool glTFGraphicsPassLighting::TryProcessSceneObject(glTFRenderResourceManager& resource_manager,
                                                   const glTFSceneObjectBase& object)
{
    const glTFLightBase* light = dynamic_cast<const glTFLightBase*>(&object);
    if (!light)
    {
        return false;
    }
    
    return GetRenderInterface<glTFRenderInterfaceLighting>()->UpdateLightInfo(*light);
}

bool glTFGraphicsPassLighting::FinishProcessSceneObject(glTFRenderResourceManager& resource_manager)
{
    RETURN_IF_FALSE(glTFGraphicsPassBase::FinishProcessSceneObject(resource_manager))

    return true;
}

bool glTFGraphicsPassLighting::SetupRootSignature(glTFRenderResourceManager& resource_manager)
{
    RETURN_IF_FALSE(glTFGraphicsPassPostprocess::SetupRootSignature(resource_manager))

    RETURN_IF_FALSE(m_root_signature_helper.AddTableRootParameter("ALBEDO_TEX_REGISTER_INDEX", {RHIDescriptorRangeType::SRV, 1, false, false}, m_albedo_allocation))
    RETURN_IF_FALSE(m_root_signature_helper.AddTableRootParameter("DEPTH_TEX_REGISTER_INDEX", {RHIDescriptorRangeType::SRV, 1, false, false}, m_depth_allocation))
    RETURN_IF_FALSE(m_root_signature_helper.AddTableRootParameter("NORMAL_TEX_REGISTER_INDEX", {RHIDescriptorRangeType::SRV, 1, false, false}, m_normal_allocation))
    
    
    return true;
}

bool glTFGraphicsPassLighting::SetupPipelineStateObject(glTFRenderResourceManager& resource_manager)
{
    RETURN_IF_FALSE(glTFGraphicsPassPostprocess::SetupPipelineStateObject(resource_manager))

    std::vector<IRHIDescriptorAllocation*> render_targets;
    render_targets.push_back(&resource_manager.GetCurrentFrameSwapChainRTV());
    GetGraphicsPipelineStateObject().BindRenderTargetFormats(render_targets);
    
    GetGraphicsPipelineStateObject().BindShaderCode(
        R"(glTFResources\ShaderSource\LightPassVS.hlsl)", RHIShaderType::Vertex, "main");
    GetGraphicsPipelineStateObject().BindShaderCode(
        R"(glTFResources\ShaderSource\LightPassPS.hlsl)", RHIShaderType::Pixel, "main");
    
    auto& shaderMacros = GetGraphicsPipelineStateObject().GetShaderMacros();
    
    // Add albedo, normal, depth register define
    m_albedo_allocation.AddShaderDefine(shaderMacros);
    m_depth_allocation.AddShaderDefine(shaderMacros);
    m_normal_allocation.AddShaderDefine(shaderMacros);
    
    return true;
}

bool glTFGraphicsPassLighting::InitResourceTable(glTFRenderResourceManager& resource_manager)
{
    RETURN_IF_FALSE(glTFGraphicsPassPostprocess::InitResourceTable(resource_manager))

    auto albedo_desc = RHITextureDesc::MakeBasePassAlbedoTextureDesc(resource_manager);
    AddImportTextureResource(RenderPassResourceTableId::BasePass_Albedo, albedo_desc, 
    {albedo_desc.GetDataFormat(), RHIResourceDimension::TEXTURE2D, RHIViewType::RVT_SRV});

    auto normal_desc = RHITextureDesc::MakeBasePassNormalTextureDesc(resource_manager);
    AddImportTextureResource(RenderPassResourceTableId::BasePass_Normal, normal_desc, 
    {normal_desc.GetDataFormat(), RHIResourceDimension::TEXTURE2D, RHIViewType::RVT_SRV});

    auto depth_desc = RHITextureDesc::MakeDepthTextureDesc(resource_manager);
    AddImportTextureResource(RenderPassResourceTableId::Depth, depth_desc,
    {RHIDataFormat::D32_SAMPLE_RESERVED, RHIResourceDimension::TEXTURE2D, RHIViewType::RVT_SRV});
    
    return true;
}
