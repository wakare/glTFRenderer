#include "glTFRenderPassLighting.h"
#include "glTFRenderResourceManager.h"
#include "../glTFRHI/RHIInterface/IRHIPipelineStateObject.h"
#include "../glTFRHI/RHIInterface/IRHIRenderTargetManager.h"
#include "../glTFRHI/RHIUtils.h"
#include "../glTFLight/glTFDirectionalLight.h"
#include "../glTFLight/glTFLightBase.h"
#include "../glTFLight/glTFPointLight.h"
#include "../glTFRHI/RHIResourceFactory.h"
#include "../glTFUtils/glTFLog.h"

glTFRenderPassLighting::glTFRenderPassLighting()
    : glTFRenderPassInterfaceSceneView(LightPass_RootParameter_SceneViewCBV, LightPass_RootParameter_SceneViewCBV)
    , m_base_pass_color_RT(nullptr)
    , m_normal_RT(nullptr)
    , m_base_pass_color_RT_SRV_Handle(0)
    , m_depth_RT_SRV_Handle(0)
    , m_normal_RT_SRV_Handle(0)
    , m_constant_buffer_per_light_draw({})
{
}

const char* glTFRenderPassLighting::PassName()
{
    return "LightPass";
}

bool glTFRenderPassLighting::InitPass(glTFRenderResourceManager& resourceManager)
{
    RETURN_IF_FALSE(glTFRenderPassPostprocess::InitPass(resourceManager))
    
    RETURN_IF_FALSE(glTFRenderPassInterfaceSceneView::InitInterface(resourceManager))

    m_light_info_GPU_constant_buffer = RHIResourceFactory::CreateRHIResource<IRHIGPUBuffer>();
    m_point_light_info_GPU_structured_buffer = RHIResourceFactory::CreateRHIResource<IRHIGPUBuffer>();
    m_directional_light_info_GPU_structured_buffer = RHIResourceFactory::CreateRHIResource<IRHIGPUBuffer>();
    
    // TODO: Calculate mesh constant buffer size
    RETURN_IF_FALSE(m_light_info_GPU_constant_buffer->InitGPUBuffer(resourceManager.GetDevice(),
        {
            L"LightPass_LightInfoConstantBuffer",
            static_cast<size_t>(64 * 1024),
            1,
            1,
            RHIBufferType::Upload,
            RHIDataFormat::Unknown,
            RHIBufferResourceType::Buffer
        }))

    RETURN_IF_FALSE(m_point_light_info_GPU_structured_buffer->InitGPUBuffer(resourceManager.GetDevice(),
        {
            L"LightPass_PointLightInfoStructuredBuffer",
            static_cast<size_t>(64 * 1024),
            1,
            1,
            RHIBufferType::Upload,
            RHIDataFormat::R32G32B32A32_FLOAT,
            RHIBufferResourceType::Buffer
        }))

    RETURN_IF_FALSE(m_directional_light_info_GPU_structured_buffer->InitGPUBuffer(resourceManager.GetDevice(),
        {
            L"LightPass_DirectionalLightInfoStructuredBuffer",
            static_cast<size_t>(64 * 1024),
            1,
            1,
            RHIBufferType::Upload,
            RHIDataFormat::R32G32B32A32_FLOAT,
            RHIBufferResourceType::Buffer
        }))

    return true;
}

bool glTFRenderPassLighting::RenderPass(glTFRenderResourceManager& resourceManager)
{
    RETURN_IF_FALSE(glTFRenderPassPostprocess::RenderPass(resourceManager))

    RETURN_IF_FALSE(glTFRenderPassInterfaceSceneView::ApplyInterface(resourceManager))
    
    RETURN_IF_FALSE(RHIUtils::Instance().AddRenderTargetBarrierToCommandList(resourceManager.GetCommandListForRecord(), *m_base_pass_color_RT,
        RHIResourceStateType::RENDER_TARGET, RHIResourceStateType::PIXEL_SHADER_RESOURCE))

    RETURN_IF_FALSE(RHIUtils::Instance().AddRenderTargetBarrierToCommandList(resourceManager.GetCommandListForRecord(), *m_normal_RT,
        RHIResourceStateType::RENDER_TARGET, RHIResourceStateType::PIXEL_SHADER_RESOURCE))
    
    RETURN_IF_FALSE(RHIUtils::Instance().AddRenderTargetBarrierToCommandList(resourceManager.GetCommandListForRecord(), resourceManager.GetDepthRT(),
        RHIResourceStateType::DEPTH_WRITE, RHIResourceStateType::PIXEL_SHADER_RESOURCE))

    RETURN_IF_FALSE(RHIUtils::Instance().SetDescriptorTableGPUHandleToRootParameterSlot(resourceManager.GetCommandListForRecord(),
        LightPass_RootParameter_BaseColorAndDepthSRV, m_main_descriptor_heap->GetGPUHandle(0)))    

    RETURN_IF_FALSE(resourceManager.GetRenderTargetManager().BindRenderTarget(resourceManager.GetCommandListForRecord(),
        {&resourceManager.GetCurrentFrameSwapchainRT()}, nullptr))

    RETURN_IF_FALSE(resourceManager.GetRenderTargetManager().ClearRenderTarget(resourceManager.GetCommandListForRecord(),
        {&resourceManager.GetCurrentFrameSwapchainRT()}))

    RETURN_IF_FALSE(UploadLightInfoGPUBuffer())
    RETURN_IF_FALSE(RHIUtils::Instance().SetConstantBufferViewGPUHandleToRootParameterSlot(resourceManager.GetCommandListForRecord(),
        LightPass_RootParameter_LightInfosCBV, m_light_info_GPU_constant_buffer->GetGPUBufferHandle()))
    
    RHIUtils::Instance().SetShaderResourceViewGPUHandleToRootParameterSlot(resourceManager.GetCommandListForRecord(),
        LightPass_RootParameter_PointLightStructuredBuffer, m_point_light_info_GPU_structured_buffer->GetGPUBufferHandle());
    
    RHIUtils::Instance().SetShaderResourceViewGPUHandleToRootParameterSlot(resourceManager.GetCommandListForRecord(),
        LightPass_RootParameter_DirectionalLightStructuredBuffer, m_directional_light_info_GPU_structured_buffer->GetGPUBufferHandle());
    
    DrawPostprocessQuad(resourceManager);
    
    RETURN_IF_FALSE(RHIUtils::Instance().AddRenderTargetBarrierToCommandList(resourceManager.GetCommandListForRecord(), *m_base_pass_color_RT,
        RHIResourceStateType::PIXEL_SHADER_RESOURCE, RHIResourceStateType::RENDER_TARGET))

    RETURN_IF_FALSE(RHIUtils::Instance().AddRenderTargetBarrierToCommandList(resourceManager.GetCommandListForRecord(), *m_normal_RT,
        RHIResourceStateType::PIXEL_SHADER_RESOURCE, RHIResourceStateType::RENDER_TARGET))

    RETURN_IF_FALSE(RHIUtils::Instance().AddRenderTargetBarrierToCommandList(resourceManager.GetCommandListForRecord(), resourceManager.GetDepthRT(),
        RHIResourceStateType::PIXEL_SHADER_RESOURCE, RHIResourceStateType::DEPTH_WRITE))
    
    return true;
}

bool glTFRenderPassLighting::TryProcessSceneObject(glTFRenderResourceManager& resourceManager,
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
            pointLightInfo.positionAndRadius = glm::vec4(glTF_Transform_WithTRS::GetTranslationFromMatrix(pointLight->GetTransformMatrix()), pointLight->GetRadius());
            pointLightInfo.intensityAndFalloff = {pointLight->GetIntensity(), pointLight->GetFalloff(), 0.0f, 0.0f};
            m_cache_point_lights[pointLight->GetID()] = pointLightInfo;
        }
        break;
        
    case glTFLightType::DirectionalLight:
        {
            const glTFDirectionalLight* directionalLight = dynamic_cast<const glTFDirectionalLight*>(light);
            DirectionalLightInfo directionalLightInfo{};
            directionalLightInfo.directionalAndIntensity = glm::vec4(directionalLight->GetDirection(), directionalLight->GetIntensity());
            m_cache_directional_lights[directionalLight->GetID()] = directionalLightInfo;
        }
        break;
    case glTFLightType::SpotLight: break;
    case glTFLightType::SkyLight: break;
    default: ;
    }
    
    return true;
}

bool glTFRenderPassLighting::FinishProcessSceneObject(glTFRenderResourceManager& resourceManager)
{
    RETURN_IF_FALSE(glTFRenderPassBase::FinishProcessSceneObject(resourceManager))

    // Update light info gpu buffer
    m_constant_buffer_per_light_draw.pointLightCount = m_cache_point_lights.size();
    m_constant_buffer_per_light_draw.directionalLightCount = m_cache_directional_lights.size();
    
    m_constant_buffer_per_light_draw.pointLightInfos.resize(m_constant_buffer_per_light_draw.pointLightCount);
    m_constant_buffer_per_light_draw.directionalInfos.resize(m_constant_buffer_per_light_draw.directionalLightCount);

    size_t pointLightIndex = 0;
    for (const auto& pointLight : m_cache_point_lights)
    {
        m_constant_buffer_per_light_draw.pointLightInfos[pointLightIndex++] = pointLight.second;
    }

    size_t directionalLightIndex = 0;
    for (const auto& directionalLight : m_cache_directional_lights)
    {
        m_constant_buffer_per_light_draw.directionalInfos[directionalLightIndex++] = directionalLight.second;
    }
    
    return true;
}

size_t glTFRenderPassLighting::GetMainDescriptorHeapSize()
{
    return 64;
}

bool glTFRenderPassLighting::SetupRootSignature(glTFRenderResourceManager& resourceManager)
{
    // Init root signature
    constexpr size_t rootSignatureParameterCount = LightPass_RootParameter_Num;
    constexpr size_t rootSignatureStaticSamplerCount = 1;
    RETURN_IF_FALSE(m_root_signature->AllocateRootSignatureSpace(rootSignatureParameterCount, rootSignatureStaticSamplerCount))

    RETURN_IF_FALSE(glTFRenderPassPostprocess::SetupRootSignature(resourceManager))
    RETURN_IF_FALSE(glTFRenderPassInterfaceSceneView::SetupRootSignature(*m_root_signature))
    
    const RHIRootParameterDescriptorRangeDesc SRVRangeDesc {RHIRootParameterDescriptorRangeType::SRV, 0, 3};
    m_root_signature->GetRootParameter(LightPass_RootParameter_BaseColorAndDepthSRV).InitAsDescriptorTableRange(1, &SRVRangeDesc);

    m_root_signature->GetRootParameter(LightPass_RootParameter_LightInfosCBV).InitAsCBV(1);
    m_root_signature->GetRootParameter(LightPass_RootParameter_PointLightStructuredBuffer).InitAsSRV(3);
    m_root_signature->GetRootParameter(LightPass_RootParameter_DirectionalLightStructuredBuffer).InitAsSRV(4);
    
    m_root_signature->GetStaticSampler(0).InitStaticSampler(0, RHIStaticSamplerAddressMode::Clamp, RHIStaticSamplerFilterMode::Linear);
    RETURN_IF_FALSE(m_root_signature->InitRootSignature(resourceManager.GetDevice()))
    
    return true;
}

bool glTFRenderPassLighting::SetupPipelineStateObject(glTFRenderResourceManager& resource_manager)
{
    RETURN_IF_FALSE(glTFRenderPassPostprocess::SetupPipelineStateObject(resource_manager))

    std::vector<IRHIRenderTarget*> render_targets;
    render_targets.push_back(&resource_manager.GetCurrentFrameSwapchainRT());
    m_pipeline_state_object->BindRenderTargets(render_targets);
    
    m_base_pass_color_RT = resource_manager.GetRenderTargetManager().GetRenderTargetWithTag("BasePassColor");
    RETURN_IF_FALSE(m_main_descriptor_heap->CreateShaderResourceViewInDescriptorHeap(resource_manager.GetDevice(), m_main_descriptor_heap->GetUsedDescriptorCount(),
            *m_base_pass_color_RT, {m_base_pass_color_RT->GetRenderTargetFormat(), RHIShaderVisibleViewDimension::TEXTURE2D}, m_base_pass_color_RT_SRV_Handle))
    
    RETURN_IF_FALSE(m_main_descriptor_heap->CreateShaderResourceViewInDescriptorHeap(resource_manager.GetDevice(), m_main_descriptor_heap->GetUsedDescriptorCount(),
            resource_manager.GetDepthRT(), {RHIDataFormat::R32_FLOAT, RHIShaderVisibleViewDimension::TEXTURE2D}, m_depth_RT_SRV_Handle))
    
    m_normal_RT = resource_manager.GetRenderTargetManager().GetRenderTargetWithTag("BasePassNormal");
    RETURN_IF_FALSE(m_main_descriptor_heap->CreateShaderResourceViewInDescriptorHeap(resource_manager.GetDevice(), m_main_descriptor_heap->GetUsedDescriptorCount(),
            *m_normal_RT, {m_normal_RT->GetRenderTargetFormat(), RHIShaderVisibleViewDimension::TEXTURE2D}, m_normal_RT_SRV_Handle))
    
    m_pipeline_state_object->BindShaderCode(
        R"(glTFResources\ShaderSource\LightPassVS.hlsl)", RHIShaderType::Vertex, "main");
    m_pipeline_state_object->BindShaderCode(
        R"(glTFResources\ShaderSource\LightPassPS.hlsl)", RHIShaderType::Pixel, "main");
    
    auto& shaderMacros = m_pipeline_state_object->GetShaderMacros();
    glTFRenderPassInterfaceSceneView::UpdateShaderCompileDefine(shaderMacros);
    
    RETURN_IF_FALSE (m_pipeline_state_object->InitPipelineStateObject(resource_manager.GetDevice(), *m_root_signature, resource_manager.GetSwapchain()))
    
    return true;
}

bool glTFRenderPassLighting::UploadLightInfoGPUBuffer()
{
    // First upload size of light infos
    RETURN_IF_FALSE(m_light_info_GPU_constant_buffer->UploadBufferFromCPU(&m_constant_buffer_per_light_draw, 0,
        sizeof(m_constant_buffer_per_light_draw.pointLightCount) + sizeof(m_constant_buffer_per_light_draw.directionalLightCount)))

    if (!m_constant_buffer_per_light_draw.pointLightInfos.empty())
    {
        RETURN_IF_FALSE(m_point_light_info_GPU_structured_buffer->UploadBufferFromCPU(m_constant_buffer_per_light_draw.pointLightInfos.data(),
            0, m_constant_buffer_per_light_draw.pointLightInfos.size() * sizeof(m_constant_buffer_per_light_draw.pointLightInfos[0])))
    }

    if (!m_constant_buffer_per_light_draw.directionalInfos.empty())
    {
        RETURN_IF_FALSE(m_directional_light_info_GPU_structured_buffer->UploadBufferFromCPU(m_constant_buffer_per_light_draw.directionalInfos.data(),
            0, m_constant_buffer_per_light_draw.directionalInfos.size() * sizeof(m_constant_buffer_per_light_draw.directionalInfos[0])))
    }
    
    return true;
}
