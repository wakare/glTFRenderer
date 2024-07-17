#include "glTFRayTracingPassReSTIRDirectLighting.h"

#include <imgui.h>

#include "glTFRenderPass/glTFRenderResourceManager.h"
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
    auto& command_list = resource_manager.GetCommandListForRecord();

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
    
    GetResourceTexture(RenderPassResourceTableId::RayTracingPass_ReSTIRSample_Output)->Transition(command_list, RHIResourceStateType::STATE_UNORDERED_ACCESS);
    GetResourceTexture(RenderPassResourceTableId::ScreenUVOffset)->Transition(command_list, RHIResourceStateType::STATE_UNORDERED_ACCESS);

    BindDescriptor(command_list, m_lighting_samples_allocation.parameter_index, *m_lighting_samples_handle);
    BindDescriptor(command_list, m_screen_uv_offset_allocation.parameter_index, *m_screen_uv_offset_handle);
    
    auto& GBuffer_output = resource_manager.GetCurrentFrameResourceManager().GetGBufferForRendering();
    RETURN_IF_FALSE(GBuffer_output.Bind(GetID(), GetPipelineType(), command_list, *m_descriptor_updater, resource_manager.GetGBufferAllocations().GetAllocationWithPassId(GetID())))
    RETURN_IF_FALSE(GBuffer_output.Transition(GetID(), command_list, RHIResourceStateType::STATE_UNORDERED_ACCESS))

    GetRenderInterface<glTFRenderInterfaceSingleConstantBuffer<RayTracingDIPassOptions>>()->UploadCPUBuffer(resource_manager, &m_pass_options, 0, sizeof(m_pass_options));
    
    return true;
}

bool glTFRayTracingPassReSTIRDirectLighting::PostRenderPass(glTFRenderResourceManager& resource_manager)
{
    RETURN_IF_FALSE(glTFRayTracingPassWithMesh::PostRenderPass(resource_manager))

    auto& command_list = resource_manager.GetCommandListForRecord();
    auto& GBuffer_output = resource_manager.GetCurrentFrameResourceManager().GetGBufferForRendering();
    RETURN_IF_FALSE(GBuffer_output.Transition(GetID(), command_list, RHIResourceStateType::STATE_NON_PIXEL_SHADER_RESOURCE))
    
    return true;
}

bool glTFRayTracingPassReSTIRDirectLighting::UpdateGUIWidgets()
{
    RETURN_IF_FALSE(glTFRayTracingPassWithMesh::UpdateGUIWidgets())
    
    ImGui::Checkbox("CheckVisibilityForAllCandidates", (bool*)&m_pass_options.check_visibility_for_all_candidates);
    ImGui::SliderInt("CandidateLightCount", &m_pass_options.candidate_light_count, 4, 64);
    
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

    RETURN_IF_FALSE(resource_manager.GetMemoryManager().GetDescriptorManager().CreateDescriptor(
                    resource_manager.GetDevice(),
                    GetResourceTexture(RenderPassResourceTableId::RayTracingPass_ReSTIRSample_Output),
                    {
                    GetResourceTexture(RenderPassResourceTableId::RayTracingPass_ReSTIRSample_Output)->GetTextureFormat(),
                    RHIResourceDimension::TEXTURE2D,
                    RHIViewType::RVT_UAV
                    },
                    m_lighting_samples_handle))

    RETURN_IF_FALSE(resource_manager.GetMemoryManager().GetDescriptorManager().CreateDescriptor(
                    resource_manager.GetDevice(),
                    GetResourceTexture(RenderPassResourceTableId::ScreenUVOffset),
                    {
                    GetResourceTexture(RenderPassResourceTableId::ScreenUVOffset)->GetTextureFormat(),
                    RHIResourceDimension::TEXTURE2D,
                    RHIViewType::RVT_UAV
                    },
                    m_screen_uv_offset_handle))

    for (unsigned i = 0; i < resource_manager.GetBackBufferCount(); ++i)
    {
        auto& GBuffer_output = resource_manager.GetFrameResourceManagerByIndex(i).GetGBufferForInit();
        RETURN_IF_FALSE(GBuffer_output.InitGBufferUAVs(GetID(), resource_manager))
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
    config.payload_size = sizeof(float) * 12;
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

bool glTFRayTracingPassReSTIRDirectLighting::InitResourceTable(glTFRenderResourceManager& resource_manager)
{
    RETURN_IF_FALSE(glTFRayTracingPassWithMesh::InitResourceTable(resource_manager))

    AddExportTextureResource(RHITextureDesc::MakeScreenUVOffsetTextureDesc(resource_manager), RenderPassResourceTableId::ScreenUVOffset);
    AddExportTextureResource(RHITextureDesc::MakeRayTracingSceneOutputTextureDesc(resource_manager), RenderPassResourceTableId::RayTracingPass_ReSTIRSample_Output);
    
    return true;
}
