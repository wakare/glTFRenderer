#include "glTFComputePassRayTracingPostprocess.h"

#include <imgui.h>

#include "glTFRenderPass/glTFRenderResourceManager.h"
#include "glTFRenderPass/glTFRenderInterface/glTFRenderInterfaceSceneView.h"
#include "glTFRHI/RHIUtils.h"

glTFComputePassRayTracingPostprocess::glTFComputePassRayTracingPostprocess()
    : m_dispatch_count({0, 0, 0})
    , m_accumulation_resource("ACCUMULATION_OUTPUT_REGISTER_INDEX", "ACCUMULATION_BACKBUFFER_REGISTER_INDEX")
    , m_custom_resource("CUSTOM_OUTPUT_REGISTER_INDEX", "CUSTOM_BACKBUFFER_REGISTER_INDEX")
{
    AddRenderInterface(std::make_shared<glTFRenderInterfaceSceneView>());
    AddRenderInterface(std::make_shared<glTFRenderInterfaceSingleConstantBuffer<RayTracingPostProcessPassOptions>>());
}

const char* glTFComputePassRayTracingPostprocess::PassName()
{
    return "RayTracingPostProcessPass";
}

bool glTFComputePassRayTracingPostprocess::InitPass(glTFRenderResourceManager& resource_manager)
{
    RHITextureDesc raytracing_accumulation_render_target
    {
        "RAYTRACING_ACCUMULATION_RESOURCE",
        resource_manager.GetSwapChain().GetWidth(),
        resource_manager.GetSwapChain().GetHeight(),
        RHIDataFormat::R32G32B32A32_FLOAT,
        static_cast<RHIResourceUsageFlags>(RUF_ALLOW_UAV | RUF_ALLOW_RENDER_TARGET),
        {
            .clear_format = RHIDataFormat::R32G32B32A32_FLOAT,
            .clear_color {0.0f, 0.0f, 0.0f, 0.0f}
        }
    };
    
    RETURN_IF_FALSE(m_accumulation_resource.CreateResource(resource_manager, raytracing_accumulation_render_target))

    RHITextureDesc raytracing_custom_render_target
    {
        "RAYTRACING_CUSTOM_RESOURCE",
        resource_manager.GetSwapChain().GetWidth(),
        resource_manager.GetSwapChain().GetHeight(),
        RHIDataFormat::R32G32B32A32_FLOAT,
        static_cast<RHIResourceUsageFlags>(RUF_ALLOW_UAV | RUF_ALLOW_RENDER_TARGET),
        {
            .clear_format = RHIDataFormat::R32G32B32A32_FLOAT,
            .clear_color {0.0f, 0.0f, 0.0f, 0.0f}
        }
    };
    RETURN_IF_FALSE(m_custom_resource.CreateResource(resource_manager, raytracing_custom_render_target))
    RETURN_IF_FALSE(glTFComputePassBase::InitPass(resource_manager))

    return true;
}

bool glTFComputePassRayTracingPostprocess::PreRenderPass(glTFRenderResourceManager& resource_manager)
{
    RETURN_IF_FALSE(glTFComputePassBase::PreRenderPass(resource_manager))

    auto& command_list = resource_manager.GetCommandListForRecord();
    
    RETURN_IF_FALSE(m_accumulation_resource.BindDescriptors(command_list, GetPipelineType(), *m_descriptor_updater))
    RETURN_IF_FALSE(m_custom_resource.BindDescriptors(command_list, GetPipelineType(), *m_descriptor_updater))

    resource_manager.GetCurrentFrameSwapChainTexture().Transition(command_list, RHIResourceStateType::STATE_RENDER_TARGET);
    GetResourceTexture(RenderPassResourceTableId::ComputePass_RayTracingOutputPostProcess_Output)->Transition(
            command_list, RHIResourceStateType::STATE_UNORDERED_ACCESS);

    GetResourceTexture(RenderPassResourceTableId::ScreenUVOffset)->Transition(command_list, RHIResourceStateType::STATE_NON_PIXEL_SHADER_RESOURCE);
    GetResourceTexture(RenderPassResourceTableId::RayTracingSceneOutput)->Transition(command_list, RHIResourceStateType::STATE_NON_PIXEL_SHADER_RESOURCE);

    BindDescriptor(command_list, m_process_input_allocation.space, m_process_input_allocation.parameter_index, *m_post_process_input_handle);
    BindDescriptor(command_list, m_screen_uv_offset_allocation.space, m_screen_uv_offset_allocation.parameter_index, *m_screen_uv_offset_handle);
    BindDescriptor(command_list, m_process_output_allocation.space, m_process_output_allocation.parameter_index, *m_post_process_output_handle);
    
    RETURN_IF_FALSE(GetRenderInterface<glTFRenderInterfaceSingleConstantBuffer<RayTracingPostProcessPassOptions>>()->UploadCPUBuffer(resource_manager, &m_pass_options, 0, sizeof(m_pass_options)))
    
    return true;
}

bool glTFComputePassRayTracingPostprocess::PostRenderPass(glTFRenderResourceManager& resource_manager)
{
    RETURN_IF_FALSE(glTFComputePassBase::PostRenderPass(resource_manager))

    auto& command_list = resource_manager.GetCommandListForRecord();
    
    // Copy accumulation buffer to back buffer
    RETURN_IF_FALSE(m_accumulation_resource.PostRendering(resource_manager))
    RETURN_IF_FALSE(m_custom_resource.PostRendering(resource_manager))

    // Copy compute result to swap chain back buffer
    resource_manager.GetCurrentFrameSwapChainTexture().Transition(command_list, RHIResourceStateType::STATE_COPY_DEST);
    GetResourceTexture(RenderPassResourceTableId::ComputePass_RayTracingOutputPostProcess_Output)->Transition(
        command_list, RHIResourceStateType::STATE_COPY_SOURCE);
    
    RETURN_IF_FALSE(RHIUtils::Instance().CopyTexture(command_list,
        resource_manager.GetCurrentFrameSwapChainTexture(),
        *GetResourceTexture(RenderPassResourceTableId::ComputePass_RayTracingOutputPostProcess_Output)))
    
    return true;
}

DispatchCount glTFComputePassRayTracingPostprocess::GetDispatchCount() const
{
    return m_dispatch_count;
}

bool glTFComputePassRayTracingPostprocess::TryProcessSceneObject(glTFRenderResourceManager& resource_manager,
    const glTFSceneObjectBase& object)
{
    RETURN_IF_FALSE(glTFComputePassBase::TryProcessSceneObject(resource_manager, object))
    
    return true;
}

bool glTFComputePassRayTracingPostprocess::FinishProcessSceneObject(glTFRenderResourceManager& resource_manager)
{
    RETURN_IF_FALSE(glTFComputePassBase::FinishProcessSceneObject(resource_manager))

    return true;
}

bool glTFComputePassRayTracingPostprocess::UpdateGUIWidgets()
{
    RETURN_IF_FALSE(glTFComputePassBase::UpdateGUIWidgets())

    ImGui::Checkbox("EnablePostProcess", (bool*)&m_pass_options.enable_post_process);
    ImGui::Checkbox("UseVelocityClamp", (bool*)&m_pass_options.use_velocity_clamp);
    ImGui::SliderFloat("ReuseHistoryFactor", &m_pass_options.reuse_history_factor, 0.0, 1.0);
    ImGui::SliderInt("ColorClampRange", &m_pass_options.color_clamp_range, 0, 5);
    
    return true;
}

bool glTFComputePassRayTracingPostprocess::InitResourceTable(glTFRenderResourceManager& resource_manager)
{
    RETURN_IF_FALSE(glTFComputePassBase::InitResourceTable(resource_manager))
    AddImportTextureResource(RHITextureDesc::MakeRayTracingSceneOutputTextureDesc(resource_manager), RenderPassResourceTableId::RayTracingSceneOutput);
    AddImportTextureResource(RHITextureDesc::MakeScreenUVOffsetTextureDesc(resource_manager), RenderPassResourceTableId::ScreenUVOffset);
    AddExportTextureResource(RHITextureDesc::MakeComputePassRayTracingPostProcessOutputDesc(resource_manager), RenderPassResourceTableId::ComputePass_RayTracingOutputPostProcess_Output);
    return true;
}

bool glTFComputePassRayTracingPostprocess::SetupRootSignature(glTFRenderResourceManager& resource_manager)
{
    RETURN_IF_FALSE(glTFComputePassBase::SetupRootSignature(resource_manager))

    m_dispatch_count = {resource_manager.GetSwapChain().GetWidth() / 8, resource_manager.GetSwapChain().GetHeight() / 8, 1};
    
    RETURN_IF_FALSE(m_accumulation_resource.RegisterSignature(m_root_signature_helper))
    RETURN_IF_FALSE(m_custom_resource.RegisterSignature(m_root_signature_helper))

    auto raytracing_output = GetResourceTexture(RenderPassResourceTableId::RayTracingSceneOutput);
    auto screen_uv_offset = GetResourceTexture(RenderPassResourceTableId::ScreenUVOffset);
    
    RETURN_IF_FALSE(resource_manager.GetMemoryManager().GetDescriptorManager().CreateDescriptor(resource_manager.GetDevice(), raytracing_output,
                        {raytracing_output->GetTextureFormat(), RHIResourceDimension::TEXTURE2D, RHIViewType::RVT_SRV}, m_post_process_input_handle))

    RETURN_IF_FALSE(resource_manager.GetMemoryManager().GetDescriptorManager().CreateDescriptor(resource_manager.GetDevice(), screen_uv_offset,
                        {screen_uv_offset->GetTextureFormat(), RHIResourceDimension::TEXTURE2D, RHIViewType::RVT_SRV}, m_screen_uv_offset_handle))

    RETURN_IF_FALSE(resource_manager.GetMemoryManager().GetDescriptorManager().CreateDescriptor(resource_manager.GetDevice(), GetResourceTexture(RenderPassResourceTableId::ComputePass_RayTracingOutputPostProcess_Output),
                                {GetResourceTexture(RenderPassResourceTableId::ComputePass_RayTracingOutputPostProcess_Output)->GetTextureFormat(), RHIResourceDimension::TEXTURE2D, RHIViewType::RVT_UAV}, m_post_process_output_handle))

    RETURN_IF_FALSE(m_root_signature_helper.AddTableRootParameter("POST_PROCESS_INPUT_REGISTER_INDEX", RHIRootParameterDescriptorRangeType::SRV, 1, false, m_process_input_allocation))
    RETURN_IF_FALSE(m_root_signature_helper.AddTableRootParameter("SCREEN_UV_OFFSET_REGISTER_INDEX", RHIRootParameterDescriptorRangeType::SRV, 1, false, m_screen_uv_offset_allocation))
    RETURN_IF_FALSE(m_root_signature_helper.AddTableRootParameter("POST_PROCESS_OUTPUT_REGISTER_INDEX", RHIRootParameterDescriptorRangeType::UAV, 1, false, m_process_output_allocation))

    return true;
}

bool glTFComputePassRayTracingPostprocess::SetupPipelineStateObject(glTFRenderResourceManager& resource_manager)
{
    RETURN_IF_FALSE(glTFComputePassBase::SetupPipelineStateObject(resource_manager))

    RETURN_IF_FALSE(m_accumulation_resource.CreateDescriptors(resource_manager))
    RETURN_IF_FALSE(m_custom_resource.CreateDescriptors(resource_manager))

    GetComputePipelineStateObject().BindShaderCode(
            R"(glTFResources\ShaderSource\ComputeShader\PathTracingPostProcess.hlsl)", RHIShaderType::Compute, "main");
    
    auto& shader_macros = GetComputePipelineStateObject().GetShaderMacros();
    RETURN_IF_FALSE(m_accumulation_resource.AddShaderMacros(shader_macros))
    RETURN_IF_FALSE(m_custom_resource.AddShaderMacros(shader_macros))
    
    m_process_input_allocation.AddShaderDefine(shader_macros);
    m_screen_uv_offset_allocation.AddShaderDefine(shader_macros);
    m_process_output_allocation.AddShaderDefine(shader_macros);
    
    return true;
}