#include "glTFComputePassReSTIRDirectLighting.h"

#include <imgui.h>

#include "glTFRenderPass/glTFRenderResourceManager.h"
#include "glTFRenderPass/glTFRenderInterface/glTFRenderInterfaceFrameStat.h"
#include "glTFRenderPass/glTFRenderInterface/glTFRenderInterfaceLighting.h"
#include "glTFRenderPass/glTFRenderInterface/glTFRenderInterfaceSceneView.h"
#include "glTFRHI/RHIInterface/IRHIPipelineStateObject.h"
#include "glTFRHI/RHIInterface/IRHISwapChain.h"

glTFComputePassReSTIRDirectLighting::glTFComputePassReSTIRDirectLighting()
    : m_aggregate_samples_output("AGGREGATE_OUTPUT_REGISTER_INDEX", "AGGREGATE_BACKBUFFER_REGISTER_INDEX")
{
}

const char* glTFComputePassReSTIRDirectLighting::PassName()
{
    return "ReSTIRLightingPass";
}

bool glTFComputePassReSTIRDirectLighting::InitRenderInterface(glTFRenderResourceManager& resource_manager)
{
    RETURN_IF_FALSE(glTFComputePassBase::InitRenderInterface(resource_manager))

    AddRenderInterface(std::make_shared<glTFRenderInterfaceSceneView>());
    AddRenderInterface(std::make_shared<glTFRenderInterfaceLighting>());
    AddRenderInterface(std::make_shared<glTFRenderInterfaceFrameStat>());
    AddRenderInterface(std::make_shared<glTFRenderInterfaceSingleConstantBuffer<RayTracingDIPostProcessPassOptions>>());
    
    return true;
}

bool glTFComputePassReSTIRDirectLighting::InitPass(glTFRenderResourceManager& resource_manager)
{
    RHITextureDesc aggregate_samples_output_desc
    {
        "AGGREGATE_OUTPUT",
        resource_manager.GetSwapChain().GetWidth(),
        resource_manager.GetSwapChain().GetHeight(),
        RHIDataFormat::R32G32B32A32_FLOAT,
        static_cast<RHIResourceUsageFlags>(RUF_ALLOW_UAV | RUF_ALLOW_RENDER_TARGET),
        {
            .clear_format = RHIDataFormat::R32G32B32A32_FLOAT,
            .clear_color {0.0f, 0.0f, 0.0f, 0.0f}
        },
    };
    RETURN_IF_FALSE(m_aggregate_samples_output.CreateResource(resource_manager, aggregate_samples_output_desc))
    RETURN_IF_FALSE(glTFComputePassBase::InitPass(resource_manager))

    return true;
}

bool glTFComputePassReSTIRDirectLighting::PreRenderPass(glTFRenderResourceManager& resource_manager)
{
    RETURN_IF_FALSE(glTFComputePassBase::PreRenderPass(resource_manager))

    auto& command_list = resource_manager.GetCommandListForRecord();
    auto& GBuffer_output = resource_manager.GetCurrentFrameResourceManager().GetGBufferForRendering();
    RETURN_IF_FALSE(GBuffer_output.Transition(GetID(), command_list, RHIResourceStateType::STATE_NON_PIXEL_SHADER_RESOURCE))
    RETURN_IF_FALSE(GBuffer_output.Bind(GetID(), GetPipelineType(), command_list, *m_descriptor_updater, resource_manager.GetGBufferAllocations().GetAllocationWithPassId(GetID())))
    
    GetResourceTexture(RenderPassResourceTableId::RayTracingSceneOutput)->Transition(command_list, RHIResourceStateType::STATE_UNORDERED_ACCESS);
    GetResourceTexture(RenderPassResourceTableId::ScreenUVOffset)->Transition(command_list, RHIResourceStateType::STATE_NON_PIXEL_SHADER_RESOURCE);
    GetResourceTexture(RenderPassResourceTableId::RayTracingPass_ReSTIRSample_Output)->Transition(command_list, RHIResourceStateType::STATE_NON_PIXEL_SHADER_RESOURCE);
    
    BindDescriptor(command_list, m_lighting_samples_allocation, *GetResourceDescriptor(RenderPassResourceTableId::RayTracingPass_ReSTIRSample_Output));
    BindDescriptor(command_list, m_screen_uv_offset_allocation, *GetResourceDescriptor(RenderPassResourceTableId::ScreenUVOffset));
    BindDescriptor(command_list, m_output_allocation, *GetResourceDescriptor(RenderPassResourceTableId::RayTracingSceneOutput));

    RETURN_IF_FALSE(GetRenderInterface<glTFRenderInterfaceLighting>()->UpdateCPUBuffer(resource_manager))

    RETURN_IF_FALSE(m_aggregate_samples_output.BindDescriptors(command_list, GetPipelineType(), *m_descriptor_updater))
    GetRenderInterface<glTFRenderInterfaceSingleConstantBuffer<RayTracingDIPostProcessPassOptions>>()->UploadCPUBuffer(resource_manager, &m_pass_options, 0, sizeof(m_pass_options));

    return true;
}

bool glTFComputePassReSTIRDirectLighting::PostRenderPass(glTFRenderResourceManager& resource_manager)
{
    RETURN_IF_FALSE(glTFComputePassBase::PostRenderPass(resource_manager))
    RETURN_IF_FALSE(m_aggregate_samples_output.PostRendering(resource_manager))

    return true;
}

DispatchCount glTFComputePassReSTIRDirectLighting::GetDispatchCount(glTFRenderResourceManager& resource_manager) const
{
    return {resource_manager.GetSwapChain().GetWidth() / 8, resource_manager.GetSwapChain().GetHeight() / 8, 1};
}

bool glTFComputePassReSTIRDirectLighting::TryProcessSceneObject(glTFRenderResourceManager& resource_manager,
                                                                const glTFSceneObjectBase& object)
{
    const glTFLightBase* light = dynamic_cast<const glTFLightBase*>(&object);
    if (!light)
    {
        return false;
    }
    
    return GetRenderInterface<glTFRenderInterfaceLighting>()->UpdateLightInfo(*light);
}

bool glTFComputePassReSTIRDirectLighting::FinishProcessSceneObject(glTFRenderResourceManager& resource_manager)
{
    RETURN_IF_FALSE(glTFComputePassBase::FinishProcessSceneObject(resource_manager))

    return true;
}

bool glTFComputePassReSTIRDirectLighting::UpdateGUIWidgets()
{
    RETURN_IF_FALSE(glTFComputePassBase::UpdateGUIWidgets())

    ImGui::Checkbox("EnableSpatialReuse", (bool*)&m_pass_options.enable_spatial_reuse);
    ImGui::Checkbox("EnableTemporalReuse", (bool*)&m_pass_options.enable_temporal_reuse);
    ImGui::SliderInt("SpatialReuseRange", &m_pass_options.spatial_reuse_range, 0, 10);
    
    return true;
}

bool glTFComputePassReSTIRDirectLighting::SetupRootSignature(glTFRenderResourceManager& resource_manager)
{
    RETURN_IF_FALSE(glTFComputePassBase::SetupRootSignature(resource_manager))

    auto& allocations = resource_manager.GetGBufferAllocations();
    RETURN_IF_FALSE(allocations.InitGBufferAllocation(GetID(), m_root_signature_helper, true))
    RETURN_IF_FALSE(m_root_signature_helper.AddTableRootParameter("LIGHTING_SAMPLES_REGISTER_INDEX", {RHIDescriptorRangeType::SRV, 1, false, false}, m_lighting_samples_allocation))
    RETURN_IF_FALSE(m_root_signature_helper.AddTableRootParameter("SCREEN_UV_OFFSET_REGISTER_INDEX", {RHIDescriptorRangeType::SRV, 1, false, false}, m_screen_uv_offset_allocation))
    RETURN_IF_FALSE(m_root_signature_helper.AddTableRootParameter("OUTPUT_TEX_REGISTER_INDEX", {RHIDescriptorRangeType::UAV, 1, false, true}, m_output_allocation))
    RETURN_IF_FALSE(m_aggregate_samples_output.RegisterSignature(m_root_signature_helper))
    return true;
}

bool glTFComputePassReSTIRDirectLighting::SetupPipelineStateObject(glTFRenderResourceManager& resource_manager)
{
    RETURN_IF_FALSE(glTFComputePassBase::SetupPipelineStateObject(resource_manager))

    RETURN_IF_FALSE(m_aggregate_samples_output.CreateDescriptors(resource_manager))

    for (unsigned i = 0; i < resource_manager.GetBackBufferCount(); ++i)
    {
        auto& GBuffer_output = resource_manager.GetFrameResourceManagerByIndex(i).GetGBufferForInit();
        RETURN_IF_FALSE(GBuffer_output.InitGBufferSRVs(GetID(), resource_manager))
    }
    
    GetComputePipelineStateObject().BindShaderCode(
        R"(glTFResources\ShaderSource\ComputeShader\ReSTIRDirectLightingCS.hlsl)", RHIShaderType::Compute, "main");
    
    auto& shader_macros = GetComputePipelineStateObject().GetShaderMacros();

    // Add albedo, normal, depth register define
    const auto& allocations = resource_manager.GetGBufferAllocations();
    RETURN_IF_FALSE(allocations.UpdateShaderMacros(GetID(), shader_macros, true))

    m_lighting_samples_allocation.AddShaderDefine(shader_macros);
    m_screen_uv_offset_allocation.AddShaderDefine(shader_macros);
    m_output_allocation.AddShaderDefine(shader_macros);
    RETURN_IF_FALSE(m_aggregate_samples_output.AddShaderMacros(shader_macros))
    
    return true;
}

bool glTFComputePassReSTIRDirectLighting::InitResourceTable(glTFRenderResourceManager& resource_manager)
{
    RETURN_IF_FALSE(glTFComputePassBase::InitResourceTable(resource_manager))

    auto sample_output_desc = RHITextureDesc::MakeRayTracingPassReSTIRSampleOutputDesc(resource_manager);
    AddImportTextureResource(RenderPassResourceTableId::RayTracingPass_ReSTIRSample_Output, sample_output_desc,
        {sample_output_desc.GetDataFormat(), RHIResourceDimension::TEXTURE2D, RHIViewType::RVT_SRV});

    auto uv_offset_desc = RHITextureDesc::MakeScreenUVOffsetTextureDesc(resource_manager);
    AddImportTextureResource(RenderPassResourceTableId::ScreenUVOffset, uv_offset_desc,
        {uv_offset_desc.GetDataFormat(), RHIResourceDimension::TEXTURE2D, RHIViewType::RVT_SRV});

    auto output_desc = RHITextureDesc::MakeRayTracingSceneOutputTextureDesc(resource_manager);
    AddExportTextureResource(RenderPassResourceTableId::RayTracingSceneOutput, output_desc, 
    {output_desc.GetDataFormat(), RHIResourceDimension::TEXTURE2D, RHIViewType::RVT_UAV});
    
    return true;
}
