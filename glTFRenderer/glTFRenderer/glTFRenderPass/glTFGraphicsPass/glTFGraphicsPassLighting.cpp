#include "glTFGraphicsPassLighting.h"
#include "glTFRenderPass/glTFRenderResourceManager.h"
#include "glTFRHI/RHIInterface/IRHIPipelineStateObject.h"
#include "glTFRHI/RHIInterface/IRHIRenderTargetManager.h"
#include "glTFRHI/RHIUtils.h"
#include "glTFLight/glTFDirectionalLight.h"
#include "glTFLight/glTFLightBase.h"
#include "glTFLight/glTFPointLight.h"
#include "glTFRenderPass/glTFRenderInterface/glTFRenderInterfaceSampler.h"
#include "glTFRHI/RHIResourceFactory.h"
#include "glTFUtils/glTFLog.h"

glTFGraphicsPassLighting::glTFGraphicsPassLighting()
    : m_base_pass_color_RT(nullptr)
    , m_normal_RT(nullptr)
    , m_base_pass_color_RT_SRV_Handle(0)
    , m_depth_RT_SRV_Handle(0)
    , m_normal_RT_SRV_Handle(0)
{
    AddRenderInterface(std::make_shared<glTFRenderInterfaceSceneView>());
    AddRenderInterface(std::make_shared<glTFRenderInterfaceLighting>());
    const std::shared_ptr<glTFRenderInterfaceSampler<RHIStaticSamplerAddressMode::Clamp, RHIStaticSamplerFilterMode::Linear>> sampler_interface =
        std::make_shared<glTFRenderInterfaceSampler<RHIStaticSamplerAddressMode::Clamp, RHIStaticSamplerFilterMode::Linear>>();
    sampler_interface->SetSamplerRegisterIndexName("DEFAULT_SAMPLER_REGISTER_INDEX");
    AddRenderInterface(sampler_interface);
}

const char* glTFGraphicsPassLighting::PassName()
{
    return "LightGraphicsPass";
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
    
    RETURN_IF_FALSE(RHIUtils::Instance().AddRenderTargetBarrierToCommandList(command_list, *m_base_pass_color_RT,
        RHIResourceStateType::STATE_RENDER_TARGET, RHIResourceStateType::STATE_PIXEL_SHADER_RESOURCE))

    RETURN_IF_FALSE(RHIUtils::Instance().AddRenderTargetBarrierToCommandList(command_list, *m_normal_RT,
        RHIResourceStateType::STATE_RENDER_TARGET, RHIResourceStateType::STATE_PIXEL_SHADER_RESOURCE))
    
    RETURN_IF_FALSE(RHIUtils::Instance().AddRenderTargetBarrierToCommandList(command_list, resource_manager.GetDepthRT(),
        RHIResourceStateType::STATE_DEPTH_READ, RHIResourceStateType::STATE_PIXEL_SHADER_RESOURCE))

    RETURN_IF_FALSE(RHIUtils::Instance().SetDTToRootParameterSlot(command_list,
        m_base_color_and_depth_allocation.parameter_index, m_main_descriptor_heap->GetGPUHandle(0), GetPipelineType() == PipelineType::Graphics))    

    RETURN_IF_FALSE(resource_manager.GetRenderTargetManager().BindRenderTarget(command_list,
        {&resource_manager.GetCurrentFrameSwapchainRT()}, nullptr))

    RETURN_IF_FALSE(resource_manager.GetRenderTargetManager().ClearRenderTarget(command_list,
        {&resource_manager.GetCurrentFrameSwapchainRT()}))

    RETURN_IF_FALSE(GetRenderInterface<glTFRenderInterfaceLighting>()->UpdateCPUBuffer())

    return true;
}

bool glTFGraphicsPassLighting::PostRenderPass(glTFRenderResourceManager& resource_manager)
{
    RETURN_IF_FALSE(glTFGraphicsPassPostprocess::PostRenderPass(resource_manager))

    auto& command_list = resource_manager.GetCommandListForRecord();
    
    RETURN_IF_FALSE(RHIUtils::Instance().AddRenderTargetBarrierToCommandList(command_list, *m_base_pass_color_RT,
            RHIResourceStateType::STATE_PIXEL_SHADER_RESOURCE, RHIResourceStateType::STATE_RENDER_TARGET))

    RETURN_IF_FALSE(RHIUtils::Instance().AddRenderTargetBarrierToCommandList(command_list, *m_normal_RT,
            RHIResourceStateType::STATE_PIXEL_SHADER_RESOURCE, RHIResourceStateType::STATE_RENDER_TARGET))

    RETURN_IF_FALSE(RHIUtils::Instance().AddRenderTargetBarrierToCommandList(command_list, resource_manager.GetDepthRT(),
            RHIResourceStateType::STATE_PIXEL_SHADER_RESOURCE, RHIResourceStateType::STATE_DEPTH_READ))

    RETURN_IF_FALSE(RHIUtils::Instance().DiscardResource(command_list, *m_base_pass_color_RT))
    RETURN_IF_FALSE(RHIUtils::Instance().DiscardResource(command_list, *m_normal_RT))
    //RETURN_IF_FALSE(RHIUtils::Instance().DiscardResource(command_list, resource_manager.GetDepthRT()))
    
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

size_t glTFGraphicsPassLighting::GetMainDescriptorHeapSize()
{
    return 64;
}

bool glTFGraphicsPassLighting::SetupRootSignature(glTFRenderResourceManager& resource_manager)
{
    RETURN_IF_FALSE(glTFGraphicsPassPostprocess::SetupRootSignature(resource_manager))

    RETURN_IF_FALSE(m_root_signature_helper.AddTableRootParameter("BaseColorAndDepth", RHIRootParameterDescriptorRangeType::SRV, 3, false, m_base_color_and_depth_allocation))
    
    return true;
}

bool glTFGraphicsPassLighting::SetupPipelineStateObject(glTFRenderResourceManager& resource_manager)
{
    RETURN_IF_FALSE(glTFGraphicsPassPostprocess::SetupPipelineStateObject(resource_manager))

    std::vector<IRHIRenderTarget*> render_targets;
    render_targets.push_back(&resource_manager.GetCurrentFrameSwapchainRT());
    GetGraphicsPipelineStateObject().BindRenderTargetFormats(render_targets);
    
    m_base_pass_color_RT = resource_manager.GetRenderTargetManager().GetRenderTargetWithTag("BasePassColor");
    RETURN_IF_FALSE(m_main_descriptor_heap->CreateShaderResourceViewInDescriptorHeap(resource_manager.GetDevice(), m_main_descriptor_heap->GetUsedDescriptorCount(),
            *m_base_pass_color_RT, {m_base_pass_color_RT->GetRenderTargetFormat(), RHIResourceDimension::TEXTURE2D}, m_base_pass_color_RT_SRV_Handle))
    
    RETURN_IF_FALSE(m_main_descriptor_heap->CreateShaderResourceViewInDescriptorHeap(resource_manager.GetDevice(), m_main_descriptor_heap->GetUsedDescriptorCount(),
            resource_manager.GetDepthRT(), {RHIDataFormat::R32_FLOAT, RHIResourceDimension::TEXTURE2D}, m_depth_RT_SRV_Handle))
    
    m_normal_RT = resource_manager.GetRenderTargetManager().GetRenderTargetWithTag("BasePassNormal");
    RETURN_IF_FALSE(m_main_descriptor_heap->CreateShaderResourceViewInDescriptorHeap(resource_manager.GetDevice(), m_main_descriptor_heap->GetUsedDescriptorCount(),
            *m_normal_RT, {m_normal_RT->GetRenderTargetFormat(), RHIResourceDimension::TEXTURE2D}, m_normal_RT_SRV_Handle))
    
    GetGraphicsPipelineStateObject().BindShaderCode(
        R"(glTFResources\ShaderSource\LightPassVS.hlsl)", RHIShaderType::Vertex, "main");
    GetGraphicsPipelineStateObject().BindShaderCode(
        R"(glTFResources\ShaderSource\LightPassPS.hlsl)", RHIShaderType::Pixel, "main");
    
    auto& shaderMacros = GetGraphicsPipelineStateObject().GetShaderMacros();
    
    // Add albedo, normal, depth register define
    shaderMacros.AddSRVRegisterDefine("ALBEDO_TEX_REGISTER_INDEX", m_base_color_and_depth_allocation.register_index);
    shaderMacros.AddSRVRegisterDefine("DEPTH_TEX_REGISTER_INDEX", m_base_color_and_depth_allocation.register_index + 1);
    shaderMacros.AddSRVRegisterDefine("NORMAL_TEX_REGISTER_INDEX", m_base_color_and_depth_allocation.register_index + 2);
    
    return true;
}