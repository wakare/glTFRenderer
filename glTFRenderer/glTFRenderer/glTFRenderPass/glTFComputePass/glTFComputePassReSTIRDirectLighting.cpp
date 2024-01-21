#include "glTFComputePassReSTIRDirectLighting.h"

#include <imgui.h>

#include "glTFRenderPass/glTFRenderInterface/glTFRenderInterfaceFrameStat.h"
#include "glTFRenderPass/glTFRenderInterface/glTFRenderInterfaceLighting.h"
#include "glTFRenderPass/glTFRenderInterface/glTFRenderInterfaceSceneView.h"

glTFComputePassReSTIRDirectLighting::glTFComputePassReSTIRDirectLighting()
    : m_aggregate_samples_output("AGGREGATE_OUTPUT_REGISTER_INDEX", "AGGREGATE_BACKBUFFER_REGISTER_INDEX")
{
    AddRenderInterface(std::make_shared<glTFRenderInterfaceSceneView>());
    AddRenderInterface(std::make_shared<glTFRenderInterfaceLighting>());
    AddRenderInterface(std::make_shared<glTFRenderInterfaceFrameStat>());
    AddRenderInterface(std::make_shared<glTFRenderInterfaceSingleConstantBuffer<RayTracingDIPostProcessPassOptions>>());
}

const char* glTFComputePassReSTIRDirectLighting::PassName()
{
    return "ReSTIR_Direct_Lighting_Pass";
}

bool glTFComputePassReSTIRDirectLighting::InitPass(glTFRenderResourceManager& resource_manager)
{
    IRHIRenderTargetDesc lighting_output_desc;
    lighting_output_desc.width = resource_manager.GetSwapchain().GetWidth();
    lighting_output_desc.height = resource_manager.GetSwapchain().GetHeight();
    lighting_output_desc.name = "LightingOutput";
    lighting_output_desc.isUAV = true;
    lighting_output_desc.clearValue.clear_format = RHIDataFormat::R8G8B8A8_UNORM;
    lighting_output_desc.clearValue.clear_color = {0.0f, 0.0f, 0.0f, 0.0f};
    
    m_output = resource_manager.GetRenderTargetManager().CreateRenderTarget(
        resource_manager.GetDevice(), RHIRenderTargetType::RTV, RHIDataFormat::R8G8B8A8_UNORM, RHIDataFormat::R8G8B8A8_UNORM, lighting_output_desc);
    resource_manager.GetRenderTargetManager().RegisterRenderTargetWithTag("RayTracingOutput", m_output);
    
    IRHIRenderTargetDesc aggregate_samples_output_desc;
    aggregate_samples_output_desc.width = resource_manager.GetSwapchain().GetWidth();
    aggregate_samples_output_desc.height = resource_manager.GetSwapchain().GetHeight();
    aggregate_samples_output_desc.name = "AggregateOutput";
    aggregate_samples_output_desc.isUAV = true;
    aggregate_samples_output_desc.clearValue.clear_format = RHIDataFormat::R32G32B32A32_FLOAT;
    aggregate_samples_output_desc.clearValue.clear_color = {0.0f, 0.0f, 0.0f, 0.0f};
    
    RETURN_IF_FALSE(m_aggregate_samples_output.CreateResource(resource_manager, aggregate_samples_output_desc))
    
    auto& command_list = resource_manager.GetCommandListForRecord();
    RETURN_IF_FALSE(RHIUtils::Instance().AddRenderTargetBarrierToCommandList(command_list, *m_output, RHIResourceStateType::STATE_RENDER_TARGET, RHIResourceStateType::STATE_NON_PIXEL_SHADER_RESOURCE))

    m_lighting_samples = resource_manager.GetRenderTargetManager().GetRenderTargetWithTag("ReSTIRDirectLightingSamples");
    m_screen_uv_offset = resource_manager.GetRenderTargetManager().GetRenderTargetWithTag("RayTracingScreenUVOffset");
    
    RETURN_IF_FALSE(glTFComputePassBase::InitPass(resource_manager))

    return true;
}

bool glTFComputePassReSTIRDirectLighting::PreRenderPass(glTFRenderResourceManager& resource_manager)
{
    RETURN_IF_FALSE(glTFComputePassBase::PreRenderPass(resource_manager))

    auto& command_list = resource_manager.GetCommandListForRecord();
    auto& GBuffer_output = resource_manager.GetCurrentFrameResourceManager().GetGBufferForRendering();
    RETURN_IF_FALSE(GBuffer_output.Transition(GetID(), command_list, RHIResourceStateType::STATE_NON_PIXEL_SHADER_RESOURCE))

    RETURN_IF_FALSE(RHIUtils::Instance().SetDTToRootParameterSlot(command_list,
        m_lighting_samples_allocation.parameter_index, m_lighting_samples_handle, GetPipelineType() == PipelineType::Graphics))

    RETURN_IF_FALSE(RHIUtils::Instance().SetDTToRootParameterSlot(command_list,
        m_screen_uv_offset_allocation.parameter_index, m_screen_uv_offset_handle, GetPipelineType() == PipelineType::Graphics))

    RETURN_IF_FALSE(RHIUtils::Instance().SetDTToRootParameterSlot(command_list,
        m_output_allocation.parameter_index, m_output_handle, GetPipelineType() == PipelineType::Graphics))
    
    RETURN_IF_FALSE(GBuffer_output.Bind(GetID(), command_list, resource_manager.GetGBufferAllocations().GetAllocationWithPassId(GetID())))
    RETURN_IF_FALSE(GetRenderInterface<glTFRenderInterfaceLighting>()->UpdateCPUBuffer())

    RETURN_IF_FALSE(m_aggregate_samples_output.BindRootParameter(resource_manager))
    
    RETURN_IF_FALSE(RHIUtils::Instance().AddRenderTargetBarrierToCommandList(command_list, *m_output,
       RHIResourceStateType::STATE_NON_PIXEL_SHADER_RESOURCE, RHIResourceStateType::STATE_UNORDERED_ACCESS))

    GetRenderInterface<glTFRenderInterfaceSingleConstantBuffer<RayTracingDIPostProcessPassOptions>>()->UploadCPUBuffer(&m_pass_options, 0, sizeof(m_pass_options));
    
    return true;
}

bool glTFComputePassReSTIRDirectLighting::PostRenderPass(glTFRenderResourceManager& resource_manager)
{
    RETURN_IF_FALSE(glTFComputePassBase::PostRenderPass(resource_manager))

    auto& command_list = resource_manager.GetCommandListForRecord();
    RETURN_IF_FALSE(m_aggregate_samples_output.CopyToBackBuffer(resource_manager))

    RETURN_IF_FALSE(RHIUtils::Instance().AddRenderTargetBarrierToCommandList(command_list, *m_output,
       RHIResourceStateType::STATE_UNORDERED_ACCESS, RHIResourceStateType::STATE_NON_PIXEL_SHADER_RESOURCE))

    return true;
}

DispatchCount glTFComputePassReSTIRDirectLighting::GetDispatchCount() const
{
    return m_dispatch_count;
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
    RETURN_IF_FALSE(m_root_signature_helper.AddTableRootParameter("LightingSamples", RHIRootParameterDescriptorRangeType::SRV, 1, false, m_lighting_samples_allocation))
    RETURN_IF_FALSE(m_root_signature_helper.AddTableRootParameter("ScreenUVOffset", RHIRootParameterDescriptorRangeType::SRV, 1, false, m_screen_uv_offset_allocation))
    RETURN_IF_FALSE(m_root_signature_helper.AddTableRootParameter("Output", RHIRootParameterDescriptorRangeType::UAV, 1, true, m_output_allocation))
    RETURN_IF_FALSE(m_aggregate_samples_output.RegisterSignature(m_root_signature_helper))
    return true;
}

bool glTFComputePassReSTIRDirectLighting::SetupPipelineStateObject(glTFRenderResourceManager& resource_manager)
{
    RETURN_IF_FALSE(glTFComputePassBase::SetupPipelineStateObject(resource_manager))

    m_dispatch_count = {resource_manager.GetSwapchain().GetWidth() / 8, resource_manager.GetSwapchain().GetHeight() / 8, 1};

    RETURN_IF_FALSE(m_main_descriptor_heap->CreateUnOrderAccessViewInDescriptorHeap(resource_manager.GetDevice(), m_main_descriptor_heap->GetUsedDescriptorCount(),
                *m_output, {m_output->GetRenderTargetFormat(), RHIResourceDimension::TEXTURE2D}, m_output_handle))

    RETURN_IF_FALSE(m_main_descriptor_heap->CreateShaderResourceViewInDescriptorHeap(resource_manager.GetDevice(), m_main_descriptor_heap->GetUsedDescriptorCount(),
                    *m_lighting_samples, {m_lighting_samples->GetRenderTargetFormat(), RHIResourceDimension::TEXTURE2D}, m_lighting_samples_handle))

    RETURN_IF_FALSE(m_main_descriptor_heap->CreateShaderResourceViewInDescriptorHeap(resource_manager.GetDevice(), m_main_descriptor_heap->GetUsedDescriptorCount(),
                *m_screen_uv_offset, {m_screen_uv_offset->GetRenderTargetFormat(), RHIResourceDimension::TEXTURE2D}, m_screen_uv_offset_handle))
    
    RETURN_IF_FALSE(m_aggregate_samples_output.CreateDescriptors(resource_manager, *m_main_descriptor_heap))
    
    for (unsigned i = 0; i < resource_manager.GetBackBufferCount(); ++i)
    {
        auto& GBuffer_output = resource_manager.GetFrameResourceManagerByIndex(i).GetGBufferForInit();
        RETURN_IF_FALSE(GBuffer_output.InitGBufferSRVs(GetID(), *m_main_descriptor_heap, resource_manager))    
    }
    
    GetComputePipelineStateObject().BindShaderCode(
        R"(glTFResources\ShaderSource\ComputeShader\ReSTIRDirectLightingCS.hlsl)", RHIShaderType::Compute, "main");
    
    auto& shader_macros = GetComputePipelineStateObject().GetShaderMacros();

    // Add albedo, normal, depth register define
    const auto& allocations = resource_manager.GetGBufferAllocations();
    RETURN_IF_FALSE(allocations.UpdateShaderMacros(GetID(), shader_macros, true))

    shader_macros.AddSRVRegisterDefine("LIGHTING_SAMPLES_REGISTER_INDEX", m_lighting_samples_allocation.register_index, m_lighting_samples_allocation.space);
    shader_macros.AddSRVRegisterDefine("SCREEN_UV_OFFSET_REGISTER_INDEX", m_screen_uv_offset_allocation.register_index, m_screen_uv_offset_allocation.space);
    shader_macros.AddUAVRegisterDefine("OUTPUT_TEX_REGISTER_INDEX", m_output_allocation.register_index, m_output_allocation.space);
    RETURN_IF_FALSE(m_aggregate_samples_output.AddShaderMacros(shader_macros))
    
    return true;
}
