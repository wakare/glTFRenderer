#include "glTFRayTracingPassReSTIRDirectLighting.h"

#include <imgui.h>

#include "glTFRenderPass/glTFRenderResourceManager.h"
#include "glTFRenderPass/glTFRenderInterface/glTFRenderInterfaceSingleConstantBuffer.h"
#include "glTFRHI/RHIUtils.h"
#include "glTFRHI/RHIInterface/IRHIRenderTargetManager.h"

glTFRayTracingPassReSTIRDirectLighting::glTFRayTracingPassReSTIRDirectLighting()
{
    
}

const char* glTFRayTracingPassReSTIRDirectLighting::PassName()
{
    return "ReSTIRSamplesGenerationPass";
}

bool glTFRayTracingPassReSTIRDirectLighting::InitRenderInterface(glTFRenderResourceManager& resource_manager)
{
    RETURN_IF_FALSE(glTFRayTracingPassWithMesh::InitRenderInterface(resource_manager))
    AddRenderInterface(std::make_shared<glTFRenderInterfaceSingleConstantBuffer<RayTracingDIPassOptions>>());
    return true;
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

    BindDescriptor(command_list, m_lighting_samples_allocation, *GetResourceDescriptor(RenderPassResourceTableId::RayTracingPass_ReSTIRSample_Output));
    BindDescriptor(command_list, m_screen_uv_offset_allocation, *GetResourceDescriptor(RenderPassResourceTableId::ScreenUVOffset));
    
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
    RETURN_IF_FALSE(m_root_signature_helper.AddTableRootParameter("OUTPUT_REGISTER_INDEX", {RHIDescriptorRangeType::UAV, 1, false, false}, m_lighting_samples_allocation))
    RETURN_IF_FALSE(m_root_signature_helper.AddTableRootParameter("SCREEN_UV_OFFSET_REGISTER_INDEX", {RHIDescriptorRangeType::UAV, 1, false, false}, m_screen_uv_offset_allocation))

    auto& allocations = resource_manager.GetGBufferAllocations();
    RETURN_IF_FALSE(allocations.InitGBufferAllocation(GetID(), m_root_signature_helper, false))
    
    RETURN_IF_FALSE(glTFRayTracingPassWithMesh::SetupRootSignature(resource_manager))
    
    return true;
}

bool glTFRayTracingPassReSTIRDirectLighting::SetupPipelineStateObject(glTFRenderResourceManager& resource_manager)
{
    RETURN_IF_FALSE(glTFRayTracingPassWithMesh::SetupPipelineStateObject(resource_manager))

    for (unsigned i = 0; i < resource_manager.GetBackBufferCount(); ++i)
    {
        auto& GBuffer_output = resource_manager.GetFrameResourceManagerByIndex(i).GetGBufferForInit();
        RETURN_IF_FALSE(GBuffer_output.InitGBufferUAVs(GetID(), resource_manager))
    }
    
    GetRayTracingPipelineStateObject().BindShaderCode("glTFResources/ShaderSource/RayTracing/ReSTIRDirectLighting.hlsl",
        RHIShaderType::RayTracing, "");
    
    auto& shader_macros = GetRayTracingPipelineStateObject().GetShaderMacros();
    m_lighting_samples_allocation.AddShaderDefine(shader_macros);
    m_screen_uv_offset_allocation.AddShaderDefine(shader_macros);
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

    auto uv_offset_desc = RHITextureDesc::MakeScreenUVOffsetTextureDesc(resource_manager);
    AddExportTextureResource(RenderPassResourceTableId::ScreenUVOffset, uv_offset_desc, 
    {
                uv_offset_desc.GetDataFormat(),
                RHIResourceDimension::TEXTURE2D,
                RHIViewType::RVT_UAV
                });
    
    auto output_desc = RHITextureDesc::MakeRayTracingSceneOutputTextureDesc(resource_manager);
    AddExportTextureResource(RenderPassResourceTableId::RayTracingPass_ReSTIRSample_Output, output_desc, 
    {
                    output_desc.GetDataFormat(),
                    RHIResourceDimension::TEXTURE2D,
                    RHIViewType::RVT_UAV
                    });
    
    return true;
}
