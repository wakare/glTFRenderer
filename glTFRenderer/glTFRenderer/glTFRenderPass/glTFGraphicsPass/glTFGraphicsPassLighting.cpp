#include "glTFGraphicsPassLighting.h"
#include "glTFRHI/RHIInterface/IRHIPipelineStateObject.h"
#include "glTFLight/glTFLightBase.h"
#include "glTFRenderPass/glTFRenderResourceManager.h"
#include "glTFRenderPass/glTFRenderInterface/glTFRenderInterfaceSampler.h"
#include "glTFRenderPass/glTFRenderInterface/glTFRenderInterfaceSceneView.h"

glTFGraphicsPassLighting::glTFGraphicsPassLighting()
    : m_base_pass_albedo_allocation(nullptr)
    , m_depth_texture_allocation(nullptr)
    , m_base_pass_normal_allocation(nullptr)
{
    AddRenderInterface(std::make_shared<glTFRenderInterfaceSceneView>());
    AddRenderInterface(std::make_shared<glTFRenderInterfaceLighting>());
    const std::shared_ptr<glTFRenderInterfaceSampler<RHIStaticSamplerAddressMode::Clamp, RHIStaticSamplerFilterMode::Linear>> sampler_interface =
        std::make_shared<glTFRenderInterfaceSampler<RHIStaticSamplerAddressMode::Clamp, RHIStaticSamplerFilterMode::Linear>>("DEFAULT_SAMPLER_REGISTER_INDEX");
    AddRenderInterface(sampler_interface);
}

const char* glTFGraphicsPassLighting::PassName()
{
    return "GraphicsPassLighting";
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
    auto basepass_albedo = GetResourceTexture(RenderPassResourceTableId::BasePass_Albedo);
    auto basepass_normal = GetResourceTexture(RenderPassResourceTableId::BasePass_Normal);
    
    basepass_albedo->Transition(command_list, RHIResourceStateType::STATE_PIXEL_SHADER_RESOURCE);
    basepass_normal->Transition(command_list, RHIResourceStateType::STATE_PIXEL_SHADER_RESOURCE);
    resource_manager.GetDepthTextureRef().Transition(command_list, RHIResourceStateType::STATE_PIXEL_SHADER_RESOURCE);

    BindDescriptor(command_list, m_albedo_allocation.space, m_albedo_allocation.global_parameter_index, *m_base_pass_albedo_allocation);
    BindDescriptor(command_list, m_depth_allocation.space, m_depth_allocation.global_parameter_index, *m_depth_texture_allocation);
    BindDescriptor(command_list, m_normal_allocation.space, m_normal_allocation.global_parameter_index, *m_base_pass_normal_allocation);

    RETURN_IF_FALSE(GetRenderInterface<glTFRenderInterfaceLighting>()->UpdateCPUBuffer(resource_manager))

    m_begin_rendering_info.m_render_targets = {&resource_manager.GetCurrentFrameSwapChainRTV()};
    m_begin_rendering_info.enable_depth_write = false;
    
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

    RETURN_IF_FALSE(m_root_signature_helper.AddTableRootParameter("ALBEDO_TEX_REGISTER_INDEX", RHIRootParameterDescriptorRangeType::SRV, 1, false, m_albedo_allocation))
    RETURN_IF_FALSE(m_root_signature_helper.AddTableRootParameter("DEPTH_TEX_REGISTER_INDEX", RHIRootParameterDescriptorRangeType::SRV, 1, false, m_depth_allocation))
    RETURN_IF_FALSE(m_root_signature_helper.AddTableRootParameter("NORMAL_TEX_REGISTER_INDEX", RHIRootParameterDescriptorRangeType::SRV, 1, false, m_normal_allocation))
    
    
    return true;
}

bool glTFGraphicsPassLighting::SetupPipelineStateObject(glTFRenderResourceManager& resource_manager)
{
    RETURN_IF_FALSE(glTFGraphicsPassPostprocess::SetupPipelineStateObject(resource_manager))

    std::vector<IRHIDescriptorAllocation*> render_targets;
    render_targets.push_back(&resource_manager.GetCurrentFrameSwapChainRTV());
    GetGraphicsPipelineStateObject().BindRenderTargetFormats(render_targets);
    
    auto basepass_albedo = GetResourceTexture(RenderPassResourceTableId::BasePass_Albedo);
    auto basepass_normal = GetResourceTexture(RenderPassResourceTableId::BasePass_Normal);
    
    RETURN_IF_FALSE(resource_manager.GetMemoryManager().GetDescriptorManager().CreateDescriptor(resource_manager.GetDevice(), basepass_albedo,
                        {basepass_albedo->GetTextureFormat(), RHIResourceDimension::TEXTURE2D, RHIViewType::RVT_SRV}, m_base_pass_albedo_allocation))

    RETURN_IF_FALSE(resource_manager.GetMemoryManager().GetDescriptorManager().CreateDescriptor(resource_manager.GetDevice(), resource_manager.GetDepthTexture(),
                        {RHIDataFormat::R32_FLOAT, RHIResourceDimension::TEXTURE2D, RHIViewType::RVT_SRV}, m_depth_texture_allocation))

    RETURN_IF_FALSE(resource_manager.GetMemoryManager().GetDescriptorManager().CreateDescriptor(resource_manager.GetDevice(), basepass_normal,
                        {basepass_normal->GetTextureFormat(), RHIResourceDimension::TEXTURE2D, RHIViewType::RVT_SRV}, m_base_pass_normal_allocation))

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

    AddImportTextureResource(RHITextureDesc::MakeBasePassAlbedoTextureDesc(resource_manager), RenderPassResourceTableId::BasePass_Albedo);
    AddImportTextureResource(RHITextureDesc::MakeBasePassNormalTextureDesc(resource_manager), RenderPassResourceTableId::BasePass_Normal);
    
    return true;
}
