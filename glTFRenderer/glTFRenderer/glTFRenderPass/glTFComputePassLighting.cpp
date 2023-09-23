#include "glTFComputePassLighting.h"
#include "../glTFLight/glTFDirectionalLight.h"
#include "../glTFLight/glTFLightBase.h"
#include "../glTFLight/glTFPointLight.h"

const char* glTFComputePassLighting::PassName()
{
    return "LightComputePass";
}

bool glTFComputePassLighting::InitPass(glTFRenderResourceManager& resource_manager)
{
    IRHIRenderTargetDesc lighting_output_desc;
    lighting_output_desc.width = resource_manager.GetSwapchain().GetWidth();
    lighting_output_desc.height = resource_manager.GetSwapchain().GetHeight();
    lighting_output_desc.name = "LightingOutput";
    lighting_output_desc.isUAV = true;
    lighting_output_desc.clearValue.clearColor = {0.0f, 0.0f, 0.0f, 0.0f};
    
    m_lighting_output_RT = resource_manager.GetRenderTargetManager().CreateRenderTarget(
        resource_manager.GetDevice(), RHIRenderTargetType::RTV, RHIDataFormat::R8G8B8A8_UNORM, RHIDataFormat::R8G8B8A8_UNORM, lighting_output_desc);
    
    RETURN_IF_FALSE(glTFComputePassBase::InitPass(resource_manager))
    
    RETURN_IF_FALSE(glTFRenderInterfaceSceneView::InitInterface(resource_manager))

    RETURN_IF_FALSE(glTFRenderInterfaceLighting::InitInterface(resource_manager))

    return true;
}

bool glTFComputePassLighting::PreRenderPass(glTFRenderResourceManager& resource_manager)
{
    RETURN_IF_FALSE(glTFComputePassBase::PreRenderPass(resource_manager))

    RETURN_IF_FALSE(glTFRenderInterfaceSceneView::ApplyInterface(resource_manager, GetPipelineType() == PipelineType::Graphics))

    auto& command_list = resource_manager.GetCommandListForRecord();

    RETURN_IF_FALSE(RHIUtils::Instance().AddRenderTargetBarrierToCommandList(command_list, *m_lighting_output_RT,
        RHIResourceStateType::RENDER_TARGET, RHIResourceStateType::UNORDER_ACCESS))
    
    RETURN_IF_FALSE(RHIUtils::Instance().AddRenderTargetBarrierToCommandList(command_list, *m_base_color_RT,
        RHIResourceStateType::RENDER_TARGET, RHIResourceStateType::PIXEL_SHADER_RESOURCE))

    RETURN_IF_FALSE(RHIUtils::Instance().AddRenderTargetBarrierToCommandList(command_list, *m_normal_RT,
        RHIResourceStateType::RENDER_TARGET, RHIResourceStateType::PIXEL_SHADER_RESOURCE))
    
    RETURN_IF_FALSE(RHIUtils::Instance().AddRenderTargetBarrierToCommandList(command_list, resource_manager.GetDepthRT(),
        RHIResourceStateType::DEPTH_READ, RHIResourceStateType::PIXEL_SHADER_RESOURCE))

    RETURN_IF_FALSE(RHIUtils::Instance().SetDTToRootParameterSlot(command_list,
        LightPass_RootParameter_BaseColorAndDepthSRV, m_main_descriptor_heap->GetGPUHandle(0), GetPipelineType() == PipelineType::Graphics))    

    RHIUtils::Instance().SetDTToRootParameterSlot(command_list,
        LightPass_RootParameter_OutputUAV,
        m_main_descriptor_heap->GetGPUHandle(3),
        GetPipelineType() == PipelineType::Graphics);

    RETURN_IF_FALSE(UploadLightInfoGPUBuffer())
    RETURN_IF_FALSE(glTFRenderInterfaceLighting::ApplyInterface(resource_manager, GetPipelineType() == PipelineType::Graphics))

    return true;
}

bool glTFComputePassLighting::PostRenderPass(glTFRenderResourceManager& resource_manager)
{
    RETURN_IF_FALSE(glTFComputePassBase::PostRenderPass(resource_manager))

    auto& command_list = resource_manager.GetCommandListForRecord();

    RETURN_IF_FALSE(RHIUtils::Instance().AddRenderTargetBarrierToCommandList(command_list, *m_lighting_output_RT,
        RHIResourceStateType::UNORDER_ACCESS, RHIResourceStateType::RENDER_TARGET))
    
    RETURN_IF_FALSE(RHIUtils::Instance().AddRenderTargetBarrierToCommandList(command_list, *m_base_color_RT,
        RHIResourceStateType::PIXEL_SHADER_RESOURCE, RHIResourceStateType::RENDER_TARGET))

    RETURN_IF_FALSE(RHIUtils::Instance().AddRenderTargetBarrierToCommandList(command_list, *m_normal_RT,
        RHIResourceStateType::PIXEL_SHADER_RESOURCE, RHIResourceStateType::RENDER_TARGET))

    RETURN_IF_FALSE(RHIUtils::Instance().AddRenderTargetBarrierToCommandList(command_list, resource_manager.GetDepthRT(),
        RHIResourceStateType::PIXEL_SHADER_RESOURCE, RHIResourceStateType::DEPTH_READ))

    RETURN_IF_FALSE(RHIUtils::Instance().DiscardResource(command_list, *m_base_color_RT))
    RETURN_IF_FALSE(RHIUtils::Instance().DiscardResource(command_list, *m_normal_RT))
    
    return true;
}

DispatchCount glTFComputePassLighting::GetDispatchCount() const
{
    return m_dispatch_count;
}

bool glTFComputePassLighting::TryProcessSceneObject(glTFRenderResourceManager& resource_manager,
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

bool glTFComputePassLighting::FinishProcessSceneObject(glTFRenderResourceManager& resource_manager)
{
    RETURN_IF_FALSE(glTFComputePassBase::FinishProcessSceneObject(resource_manager))

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

size_t glTFComputePassLighting::GetRootSignatureParameterCount()
{
    return LightPass_RootParameter_Num;
}

size_t glTFComputePassLighting::GetRootSignatureSamplerCount()
{
    return 1;
}

size_t glTFComputePassLighting::GetMainDescriptorHeapSize()
{
    return 64;
}

bool glTFComputePassLighting::SetupRootSignature(glTFRenderResourceManager& resource_manager)
{
    RETURN_IF_FALSE(glTFComputePassBase::SetupRootSignature(resource_manager))
    RETURN_IF_FALSE(glTFRenderInterfaceSceneView::ApplyRootSignature(*m_root_signature))
    RETURN_IF_FALSE(glTFRenderInterfaceLighting::ApplyRootSignature(*m_root_signature))
    
    const RHIRootParameterDescriptorRangeDesc SRVRangeDesc {RHIRootParameterDescriptorRangeType::SRV, 0, 3};
    m_root_signature->GetRootParameter(LightPass_RootParameter_BaseColorAndDepthSRV).InitAsDescriptorTableRange(1, &SRVRangeDesc);

    const RHIRootParameterDescriptorRangeDesc UAVRangeDesc {RHIRootParameterDescriptorRangeType::UAV, 0, 1};
    m_root_signature->GetRootParameter(LightPass_RootParameter_OutputUAV).InitAsDescriptorTableRange(1, &UAVRangeDesc);
    
    m_root_signature->GetStaticSampler(0).InitStaticSampler(0, RHIStaticSamplerAddressMode::Clamp, RHIStaticSamplerFilterMode::Linear);

    return true;
}

bool glTFComputePassLighting::SetupPipelineStateObject(glTFRenderResourceManager& resource_manager)
{
    RETURN_IF_FALSE(glTFComputePassBase::SetupPipelineStateObject(resource_manager))

    m_dispatch_count = {resource_manager.GetSwapchain().GetWidth() / 8, resource_manager.GetSwapchain().GetHeight() / 8, 1};
    
    m_base_color_RT = resource_manager.GetRenderTargetManager().GetRenderTargetWithTag("BasePassColor");
    RETURN_IF_FALSE(m_main_descriptor_heap->CreateShaderResourceViewInDescriptorHeap(resource_manager.GetDevice(), m_main_descriptor_heap->GetUsedDescriptorCount(),
            *m_base_color_RT, {m_base_color_RT->GetRenderTargetFormat(), RHIResourceDimension::TEXTURE2D}, m_base_color_SRV))
    
    RETURN_IF_FALSE(m_main_descriptor_heap->CreateShaderResourceViewInDescriptorHeap(resource_manager.GetDevice(), m_main_descriptor_heap->GetUsedDescriptorCount(),
            resource_manager.GetDepthRT(), {RHIDataFormat::R32_FLOAT, RHIResourceDimension::TEXTURE2D}, m_depth_SRV))
    
    m_normal_RT = resource_manager.GetRenderTargetManager().GetRenderTargetWithTag("BasePassNormal");
    RETURN_IF_FALSE(m_main_descriptor_heap->CreateShaderResourceViewInDescriptorHeap(resource_manager.GetDevice(), m_main_descriptor_heap->GetUsedDescriptorCount(),
            *m_normal_RT, {m_normal_RT->GetRenderTargetFormat(), RHIResourceDimension::TEXTURE2D}, m_normal_SRV))

    RETURN_IF_FALSE(m_main_descriptor_heap->CreateUnOrderAccessViewInDescriptorHeap(resource_manager.GetDevice(), m_main_descriptor_heap->GetUsedDescriptorCount(),
            *m_lighting_output_RT, {m_lighting_output_RT->GetRenderTargetFormat(), RHIResourceDimension::TEXTURE2D}, m_output_UAV))
    
    GetComputePipelineStateObject().BindShaderCode(
        R"(glTFResources\ShaderSource\ComputeShader\LightingCS.hlsl)", RHIShaderType::Compute, "main");
    
    auto& shaderMacros = GetComputePipelineStateObject().GetShaderMacros();
    glTFRenderInterfaceSceneView::UpdateShaderCompileDefine(shaderMacros);
    glTFRenderInterfaceLighting::UpdateShaderCompileDefine(shaderMacros);
}

bool glTFComputePassLighting::UploadLightInfoGPUBuffer()
{
    return glTFRenderInterfaceLighting::UpdateCPUBuffer(m_constant_buffer_per_light_draw);
}
