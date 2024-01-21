#include "glTFRayTracingPassReSTIRDirectLighting.h"

#include <imgui.h>

#include "glTFRenderPass/glTFRenderInterface/glTFRenderInterfaceSingleConstantBuffer.h"
#include "glTFRHI/RHIUtils.h"
#include "glTFRHI/RHIInterface/IRHIRenderTargetManager.h"

glTFRayTracingPassReSTIRDirectLighting::glTFRayTracingPassReSTIRDirectLighting()
    : m_lighting_samples_handle(0)
    , m_screen_uv_offset_handle(0)
{
    AddRenderInterface(std::make_shared<glTFRenderInterfaceSingleConstantBuffer<RayTracingDIPassOptions>>());
}

const char* glTFRayTracingPassReSTIRDirectLighting::PassName()
{
    return "ReSTIRSamplesGenerationPass";
}

bool glTFRayTracingPassReSTIRDirectLighting::InitPass(glTFRenderResourceManager& resource_manager)
{
    IRHIRenderTargetDesc raytracing_output_render_target;
    raytracing_output_render_target.width = resource_manager.GetSwapchain().GetWidth();
    raytracing_output_render_target.height = resource_manager.GetSwapchain().GetHeight();
    raytracing_output_render_target.name = "ReSTIRDirectLightingSamples";
    raytracing_output_render_target.isUAV = true;
    raytracing_output_render_target.clearValue.clear_format = RHIDataFormat::R32G32B32A32_FLOAT;
    raytracing_output_render_target.clearValue.clear_color = {0.0f, 0.0f, 0.0f, 0.0f};

    m_lighting_samples = resource_manager.GetRenderTargetManager().CreateRenderTarget(
                resource_manager.GetDevice(), RHIRenderTargetType::RTV, RHIDataFormat::R32G32B32A32_FLOAT, RHIDataFormat::R32G32B32A32_FLOAT, raytracing_output_render_target);
    resource_manager.GetRenderTargetManager().RegisterRenderTargetWithTag("ReSTIRDirectLightingSamples", m_lighting_samples);
    
    IRHIRenderTargetDesc screen_uv_offset_render_target;
    screen_uv_offset_render_target.width = resource_manager.GetSwapchain().GetWidth();
    screen_uv_offset_render_target.height = resource_manager.GetSwapchain().GetHeight();
    screen_uv_offset_render_target.name = "RayTracingScreenUVOffset";
    screen_uv_offset_render_target.isUAV = true;
    screen_uv_offset_render_target.clearValue.clear_format = RHIDataFormat::R32G32B32A32_FLOAT;
    screen_uv_offset_render_target.clearValue.clear_color = {0.0f, 0.0f, 0.0f, 0.0f};
    
    m_screen_uv_offset_output = resource_manager.GetRenderTargetManager().CreateRenderTarget(
                    resource_manager.GetDevice(), RHIRenderTargetType::RTV, RHIDataFormat::R32G32B32A32_FLOAT, RHIDataFormat::R32G32B32A32_FLOAT, screen_uv_offset_render_target);
    resource_manager.GetRenderTargetManager().RegisterRenderTargetWithTag("RayTracingScreenUVOffset", m_screen_uv_offset_output);

    auto& command_list = resource_manager.GetCommandListForRecord();
    RETURN_IF_FALSE(RHIUtils::Instance().AddRenderTargetBarrierToCommandList(command_list, *m_lighting_samples,
        RHIResourceStateType::STATE_RENDER_TARGET, RHIResourceStateType::STATE_NON_PIXEL_SHADER_RESOURCE))
    
    RETURN_IF_FALSE(RHIUtils::Instance().AddRenderTargetBarrierToCommandList(command_list, *m_screen_uv_offset_output,
        RHIResourceStateType::STATE_RENDER_TARGET, RHIResourceStateType::STATE_NON_PIXEL_SHADER_RESOURCE))

    for (unsigned i = 0; i < resource_manager.GetBackBufferCount(); ++i)
    {
        auto& GBuffer_output = resource_manager.GetFrameResourceManagerByIndex(i).GetGBufferForInit();
        RETURN_IF_FALSE(GBuffer_output.InitGBufferOutput(resource_manager, i))
        RETURN_IF_FALSE(GBuffer_output.Transition(GetID(), command_list, RHIResourceStateType::STATE_NON_PIXEL_SHADER_RESOURCE))    
    }
    
    RETURN_IF_FALSE(glTFRayTracingPassWithMesh::InitPass(resource_manager))
    
    return true;
}

bool glTFRayTracingPassReSTIRDirectLighting::PreRenderPass(glTFRenderResourceManager& resource_manager)
{
    RETURN_IF_FALSE(glTFRayTracingPassWithMesh::PreRenderPass(resource_manager))
    
    auto& command_list = resource_manager.GetCommandListForRecord();
    RETURN_IF_FALSE(RHIUtils::Instance().SetDTToRootParameterSlot(command_list, m_lighting_samples_allocation.parameter_index, m_lighting_samples_handle, false))
    RETURN_IF_FALSE(RHIUtils::Instance().SetDTToRootParameterSlot(command_list, m_screen_uv_offset_allocation.parameter_index, m_screen_uv_offset_handle, false))
    
    RETURN_IF_FALSE(RHIUtils::Instance().AddRenderTargetBarrierToCommandList(command_list, *m_lighting_samples,
            RHIResourceStateType::STATE_NON_PIXEL_SHADER_RESOURCE, RHIResourceStateType::STATE_UNORDERED_ACCESS))

    RETURN_IF_FALSE(RHIUtils::Instance().AddRenderTargetBarrierToCommandList(command_list, *m_screen_uv_offset_output,
            RHIResourceStateType::STATE_NON_PIXEL_SHADER_RESOURCE, RHIResourceStateType::STATE_UNORDERED_ACCESS))

    auto& GBuffer_output = resource_manager.GetCurrentFrameResourceManager().GetGBufferForRendering();
    RETURN_IF_FALSE(GBuffer_output.Bind(GetID(), command_list, resource_manager.GetGBufferAllocations().GetAllocationWithPassId(GetID())))
    RETURN_IF_FALSE(GBuffer_output.Transition(GetID(), command_list, RHIResourceStateType::STATE_UNORDERED_ACCESS))

    GetRenderInterface<glTFRenderInterfaceSingleConstantBuffer<RayTracingDIPassOptions>>()->UploadCPUBuffer(&m_pass_options, 0, sizeof(m_pass_options));
    
    return true;
}

bool glTFRayTracingPassReSTIRDirectLighting::PostRenderPass(glTFRenderResourceManager& resource_manager)
{
    RETURN_IF_FALSE(glTFRayTracingPassWithMesh::PostRenderPass(resource_manager))

    auto& command_list = resource_manager.GetCommandListForRecord();

    RETURN_IF_FALSE(RHIUtils::Instance().AddRenderTargetBarrierToCommandList(command_list, *m_lighting_samples,
        RHIResourceStateType::STATE_UNORDERED_ACCESS, RHIResourceStateType::STATE_NON_PIXEL_SHADER_RESOURCE))

    RETURN_IF_FALSE(RHIUtils::Instance().AddRenderTargetBarrierToCommandList(command_list, *m_screen_uv_offset_output,
        RHIResourceStateType::STATE_UNORDERED_ACCESS, RHIResourceStateType::STATE_NON_PIXEL_SHADER_RESOURCE))
    
    auto& GBuffer_output = resource_manager.GetCurrentFrameResourceManager().GetGBufferForRendering();
    RETURN_IF_FALSE(GBuffer_output.Transition(GetID(), command_list, RHIResourceStateType::STATE_NON_PIXEL_SHADER_RESOURCE))
    
    return true;
}

bool glTFRayTracingPassReSTIRDirectLighting::UpdateGUIWidgets()
{
    RETURN_IF_FALSE(glTFRayTracingPassWithMesh::UpdateGUIWidgets())
    
    ImGui::Checkbox("CheckVisibilityForAllCandidates", (bool*)&m_pass_options.check_visibility_for_all_candidates);
    ImGui::SliderInt("CandidateLightCount", &m_pass_options.candidate_light_count, 4, 16);
    
    return true;
}

bool glTFRayTracingPassReSTIRDirectLighting::SetupRootSignature(glTFRenderResourceManager& resource_manager)
{
    // Non-bindless table parameter should be added first
    RETURN_IF_FALSE(m_root_signature_helper.AddTableRootParameter("ReSTIRDirectLightingSamples", RHIRootParameterDescriptorRangeType::UAV, 1, false, m_lighting_samples_allocation))
    RETURN_IF_FALSE(m_root_signature_helper.AddTableRootParameter("RayTracingScreenUVOffset", RHIRootParameterDescriptorRangeType::UAV, 1, false, m_screen_uv_offset_allocation))

    auto& allocations = resource_manager.GetGBufferAllocations();
    RETURN_IF_FALSE(allocations.InitGBufferAllocation(GetID(), m_root_signature_helper, false))
    
    RETURN_IF_FALSE(glTFRayTracingPassWithMesh::SetupRootSignature(resource_manager))
    
    return true;
}

bool glTFRayTracingPassReSTIRDirectLighting::SetupPipelineStateObject(glTFRenderResourceManager& resource_manager)
{
    RETURN_IF_FALSE(glTFRayTracingPassWithMesh::SetupPipelineStateObject(resource_manager))

    RETURN_IF_FALSE(m_main_descriptor_heap->CreateUnOrderAccessViewInDescriptorHeap(
        resource_manager.GetDevice(),
        m_main_descriptor_heap->GetUsedDescriptorCount(),
        *m_lighting_samples,
        {
            m_lighting_samples->GetRenderTargetFormat(),
            RHIResourceDimension::TEXTURE2D
        },
        m_lighting_samples_handle))

    RETURN_IF_FALSE(m_main_descriptor_heap->CreateUnOrderAccessViewInDescriptorHeap(
        resource_manager.GetDevice(),
        m_main_descriptor_heap->GetUsedDescriptorCount(),
        *m_screen_uv_offset_output,
        {
            m_screen_uv_offset_output->GetRenderTargetFormat(),
            RHIResourceDimension::TEXTURE2D
        },
        m_screen_uv_offset_handle))

    for (unsigned i = 0; i < resource_manager.GetBackBufferCount(); ++i)
    {
        auto& GBuffer_output = resource_manager.GetFrameResourceManagerByIndex(i).GetGBufferForInit();
        RETURN_IF_FALSE(GBuffer_output.InitGBufferUAVs(GetID(), *m_main_descriptor_heap, resource_manager))
    }
    
    GetRayTracingPipelineStateObject().BindShaderCode("glTFResources/ShaderSource/RayTracing/ReSTIRDirectLighting.hlsl",
        RHIShaderType::RayTracing, "");
    
    auto& shader_macros = GetRayTracingPipelineStateObject().GetShaderMacros();
    shader_macros.AddUAVRegisterDefine("OUTPUT_REGISTER_INDEX", m_lighting_samples_allocation.register_index, m_lighting_samples_allocation.space);
    shader_macros.AddUAVRegisterDefine("SCREEN_UV_OFFSET_REGISTER_INDEX", m_screen_uv_offset_allocation.register_index, m_screen_uv_offset_allocation.space);
    const auto& allocations = resource_manager.GetGBufferAllocations();
    RETURN_IF_FALSE(allocations.UpdateShaderMacros(GetID(), shader_macros, false))
    
    // One ray type mapping one hit group desc
    GetRayTracingPipelineStateObject().AddHitGroupDesc({GetPrimaryRayHitGroupName(),
        GetPrimaryRayCHFunctionName(),
        "",
        ""
    });

    IRHIRayTracingPipelineStateObject::RHIRayTracingConfig config;
    config.payload_size = sizeof(float) * 10;
    config.attribute_size = sizeof(float) * 2;
    config.max_recursion_count = 1;
    GetRayTracingPipelineStateObject().SetConfig(config);

    GetRayTracingPipelineStateObject().SetExportFunctionNames(
        {
            GetRayGenFunctionName(),
            GetPrimaryRayCHFunctionName(),
            GetPrimaryRayMissFunctionName(),
            GetShadowRayMissFunctionName(),
        });
    
    return true;
}

const char* glTFRayTracingPassReSTIRDirectLighting::GetRayGenFunctionName()
{
    return "PathTracingRayGen";
}

const char* glTFRayTracingPassReSTIRDirectLighting::GetPrimaryRayCHFunctionName()
{
    return "PrimaryRayClosestHit";
}

const char* glTFRayTracingPassReSTIRDirectLighting::GetPrimaryRayMissFunctionName()
{
    return "PrimaryRayMiss";
}

const char* glTFRayTracingPassReSTIRDirectLighting::GetPrimaryRayHitGroupName()
{
    return "MyHitGroup";
}

const char* glTFRayTracingPassReSTIRDirectLighting::GetShadowRayMissFunctionName()
{
    return "ShadowRayMiss";
}

const char* glTFRayTracingPassReSTIRDirectLighting::GetShadowRayHitGroupName()
{
    return "ShadowHitGroup";
}