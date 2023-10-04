#include "glTFGraphicsPassLighting.h"
#include "glTFRenderPass/glTFRenderResourceManager.h"
#include "glTFRHI/RHIInterface/IRHIPipelineStateObject.h"
#include "glTFRHI/RHIInterface/IRHIRenderTargetManager.h"
#include "glTFRHI/RHIUtils.h"
#include "glTFLight/glTFDirectionalLight.h"
#include "glTFLight/glTFLightBase.h"
#include "glTFLight/glTFPointLight.h"
#include "glTFRHI/RHIResourceFactory.h"
#include "glTFUtils/glTFLog.h"

glTFGraphicsPassLighting::glTFGraphicsPassLighting()
    : m_base_pass_color_RT(nullptr)
    , m_normal_RT(nullptr)
    , m_base_pass_color_RT_SRV_Handle(0)
    , m_depth_RT_SRV_Handle(0)
    , m_normal_RT_SRV_Handle(0)
    , m_constant_buffer_per_light_draw({})
{
    AddRenderInterface(std::make_shared<glTFRenderInterfaceSceneView>());
    AddRenderInterface(std::make_shared<glTFRenderInterfaceLighting>() );
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

    RETURN_IF_FALSE(UploadLightInfoGPUBuffer())

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

    switch (light->GetType())
    {
    case glTFLightType::PointLight:
        {
            const glTFPointLight* pointLight = dynamic_cast<const glTFPointLight*>(light);
            PointLightInfo pointLightInfo{};
            pointLightInfo.position_and_radius = glm::vec4(glTF_Transform_WithTRS::GetTranslationFromMatrix(pointLight->GetTransformMatrix()), pointLight->GetRadius());
            pointLightInfo.intensity_and_falloff = {pointLight->GetIntensity(), pointLight->GetFalloff(), 0.0f, 0.0f};
            m_cache_point_lights[pointLight->GetID()] = pointLightInfo;
        }
        break;
        
    case glTFLightType::DirectionalLight:
        {
            const glTFDirectionalLight* directionalLight = dynamic_cast<const glTFDirectionalLight*>(light);
            DirectionalLightInfo directionalLightInfo{};
            directionalLightInfo.directional_and_intensity = glm::vec4(directionalLight->GetDirection(), directionalLight->GetIntensity());
            m_cache_directional_lights[directionalLight->GetID()] = directionalLightInfo;
        }
        break;
    case glTFLightType::SpotLight: break;
    case glTFLightType::SkyLight: break;
    default: ;
    }
    
    return true;
}

bool glTFGraphicsPassLighting::FinishProcessSceneObject(glTFRenderResourceManager& resource_manager)
{
    RETURN_IF_FALSE(glTFGraphicsPassBase::FinishProcessSceneObject(resource_manager))

    // Update light info gpu buffer
    m_constant_buffer_per_light_draw.light_info.point_light_count = m_cache_point_lights.size();
    m_constant_buffer_per_light_draw.light_info.directional_light_count = m_cache_directional_lights.size();
    
    m_constant_buffer_per_light_draw.point_light_infos.resize(m_constant_buffer_per_light_draw.light_info.point_light_count);
    m_constant_buffer_per_light_draw.directional_light_infos.resize(m_constant_buffer_per_light_draw.light_info.directional_light_count);

    size_t pointLightIndex = 0;
    for (const auto& pointLight : m_cache_point_lights)
    {
        m_constant_buffer_per_light_draw.point_light_infos[pointLightIndex++] = pointLight.second;
    }

    size_t directionalLightIndex = 0;
    for (const auto& directionalLight : m_cache_directional_lights)
    {
        m_constant_buffer_per_light_draw.directional_light_infos[directionalLightIndex++] = directionalLight.second;
    }
    
    return true;
}

size_t glTFGraphicsPassLighting::GetMainDescriptorHeapSize()
{
    return 64;
}

bool glTFGraphicsPassLighting::SetupRootSignature(glTFRenderResourceManager& resource_manager)
{
    RETURN_IF_FALSE(glTFGraphicsPassPostprocess::SetupRootSignature(resource_manager))

    RETURN_IF_FALSE(m_root_signature_helper.AddTableRootParameter("BaseColorAndDepth", RHIRootParameterDescriptorRangeType::SRV, 3, m_base_color_and_depth_allocation))
    RETURN_IF_FALSE(m_root_signature_helper.AddSampler("DefaultSampler", RHIStaticSamplerAddressMode::Clamp, RHIStaticSamplerFilterMode::Linear, m_sampler_allocation))
    
    return true;
}

bool glTFGraphicsPassLighting::SetupPipelineStateObject(glTFRenderResourceManager& resource_manager)
{
    RETURN_IF_FALSE(glTFGraphicsPassPostprocess::SetupPipelineStateObject(resource_manager))

    std::vector<IRHIRenderTarget*> render_targets;
    render_targets.push_back(&resource_manager.GetCurrentFrameSwapchainRT());
    GetGraphicsPipelineStateObject().BindRenderTargets(render_targets);
    
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
    shaderMacros.AddSamplerRegisterDefine("DEFAULT_SAMPLER_REGISTER_INDEX", m_sampler_allocation.register_index);
    
    return true;
}

bool glTFGraphicsPassLighting::UploadLightInfoGPUBuffer()
{
    return GetRenderInterface<glTFRenderInterfaceLighting>()->UpdateCPUBuffer(m_constant_buffer_per_light_draw);
}
