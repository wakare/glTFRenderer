#include "glTFComputePassRayTracingPostprocess.h"

#include "glTFRenderPass/glTFRenderInterface/glTFRenderInterfaceSceneView.h"
#include "glTFRHI/RHIUtils.h"
#include "glTFRHI/RHIInterface/IRHIRenderTargetManager.h"

glTFComputePassRayTracingPostprocess::glTFComputePassRayTracingPostprocess()
    : m_dispatch_count({0, 0, 0})
    , m_accumulation_resource("ACCUMULATION_OUTPUT_REGISTER_INDEX", "ACCUMULATION_BACKBUFFER_REGISTER_INDEX")
    , m_custom_resource("CUSTOM_OUTPUT_REGISTER_INDEX", "CUSTOM_BACKBUFFER_REGISTER_INDEX")
{
    AddRenderInterface(std::make_shared<glTFRenderInterfaceSceneView>());
}

const char* glTFComputePassRayTracingPostprocess::PassName()
{
    return "RayTracingPostProcessPass";
}

bool glTFComputePassRayTracingPostprocess::InitPass(glTFRenderResourceManager& resource_manager)
{
    IRHIRenderTargetDesc raytracing_accumulation_render_target;
    raytracing_accumulation_render_target.width = resource_manager.GetSwapchain().GetWidth();
    raytracing_accumulation_render_target.height = resource_manager.GetSwapchain().GetHeight();
    raytracing_accumulation_render_target.name = "RAYTRACING_ACCUMULATION_RESOURCE";
    raytracing_accumulation_render_target.isUAV = true;
    raytracing_accumulation_render_target.clearValue.clear_format = RHIDataFormat::R32G32B32A32_FLOAT;
    raytracing_accumulation_render_target.clearValue.clear_color = {0.0f, 0.0f, 0.0f, 0.0f};
    
    RETURN_IF_FALSE(m_accumulation_resource.CreateResource(resource_manager, raytracing_accumulation_render_target))

    IRHIRenderTargetDesc raytracing_custom_render_target;
    raytracing_custom_render_target.width = resource_manager.GetSwapchain().GetWidth();
    raytracing_custom_render_target.height = resource_manager.GetSwapchain().GetHeight();
    raytracing_custom_render_target.name = "RAYTRACING_CUSTOM_RESOURCE";
    raytracing_custom_render_target.isUAV = true;
    raytracing_custom_render_target.clearValue.clear_format = RHIDataFormat::R32G32B32A32_FLOAT;
    raytracing_custom_render_target.clearValue.clear_color = {0.0f, 0.0f, 0.0f, 0.0f};
    
    RETURN_IF_FALSE(m_custom_resource.CreateResource(resource_manager, raytracing_custom_render_target))

    IRHIRenderTargetDesc post_process_output_desc;
    post_process_output_desc.width = resource_manager.GetSwapchain().GetWidth();
    post_process_output_desc.height = resource_manager.GetSwapchain().GetHeight();
    post_process_output_desc.name = "PostProcessOutput";
    post_process_output_desc.isUAV = true;
    post_process_output_desc.clearValue.clear_format = RHIDataFormat::R8G8B8A8_UNORM;
    post_process_output_desc.clearValue.clear_color = {0.0f, 0.0f, 0.0f, 0.0f};
    
    m_post_process_output_RT = resource_manager.GetRenderTargetManager().CreateRenderTarget(
        resource_manager.GetDevice(), RHIRenderTargetType::RTV, RHIDataFormat::R8G8B8A8_UNORM, RHIDataFormat::R8G8B8A8_UNORM, post_process_output_desc);
    
    RETURN_IF_FALSE(RHIUtils::Instance().AddRenderTargetBarrierToCommandList(resource_manager.GetCommandListForRecord(), *m_post_process_output_RT,
        RHIResourceStateType::STATE_RENDER_TARGET, RHIResourceStateType::STATE_UNORDERED_ACCESS))
    
    RETURN_IF_FALSE(glTFComputePassBase::InitPass(resource_manager))

    return true;
}

bool glTFComputePassRayTracingPostprocess::PreRenderPass(glTFRenderResourceManager& resource_manager)
{
    RETURN_IF_FALSE(glTFComputePassBase::PreRenderPass(resource_manager))

    RETURN_IF_FALSE(m_accumulation_resource.BindRootParameter(resource_manager))
    RETURN_IF_FALSE(m_custom_resource.BindRootParameter(resource_manager))

    auto& command_list = resource_manager.GetCommandListForRecord();
    
    RETURN_IF_FALSE(RHIUtils::Instance().AddRenderTargetBarrierToCommandList(command_list, *m_post_process_input_RT,
        RHIResourceStateType::STATE_PIXEL_SHADER_RESOURCE, RHIResourceStateType::STATE_NON_PIXEL_SHADER_RESOURCE))
    
    RETURN_IF_FALSE(RHIUtils::Instance().SetDTToRootParameterSlot(command_list,
            m_process_input_allocation.parameter_index, m_post_process_input_handle, GetPipelineType() == PipelineType::Graphics))

    RETURN_IF_FALSE(RHIUtils::Instance().SetDTToRootParameterSlot(command_list,
            m_screen_uv_offset_allocation.parameter_index, m_screen_uv_offset_handle, GetPipelineType() == PipelineType::Graphics))

    RETURN_IF_FALSE(RHIUtils::Instance().SetDTToRootParameterSlot(command_list,
            m_process_output_allocation.parameter_index, m_post_process_output_handle, GetPipelineType() == PipelineType::Graphics))

    return true;
}

bool glTFComputePassRayTracingPostprocess::PostRenderPass(glTFRenderResourceManager& resource_manager)
{
    RETURN_IF_FALSE(glTFComputePassBase::PostRenderPass(resource_manager))

    auto& command_list = resource_manager.GetCommandListForRecord();
    
    // Copy accumulation buffer to back buffer
    RETURN_IF_FALSE(m_accumulation_resource.CopyToBackBuffer(resource_manager))
    RETURN_IF_FALSE(m_custom_resource.CopyToBackBuffer(resource_manager))

    // Copy compute result to swap chain back buffer
    RETURN_IF_FALSE(RHIUtils::Instance().AddRenderTargetBarrierToCommandList(command_list, resource_manager.GetCurrentFrameSwapchainRT(),
        RHIResourceStateType::STATE_RENDER_TARGET, RHIResourceStateType::STATE_COPY_DEST))

    RETURN_IF_FALSE(RHIUtils::Instance().AddRenderTargetBarrierToCommandList(command_list, *m_post_process_output_RT,
            RHIResourceStateType::STATE_UNORDERED_ACCESS, RHIResourceStateType::STATE_COPY_SOURCE ))
    
    RETURN_IF_FALSE(RHIUtils::Instance().CopyTexture(command_list, resource_manager.GetCurrentFrameSwapchainRT(), *m_post_process_output_RT))

    RETURN_IF_FALSE(RHIUtils::Instance().AddRenderTargetBarrierToCommandList(command_list, resource_manager.GetCurrentFrameSwapchainRT(),
        RHIResourceStateType::STATE_COPY_DEST, RHIResourceStateType::STATE_RENDER_TARGET))

    RETURN_IF_FALSE(RHIUtils::Instance().AddRenderTargetBarrierToCommandList(command_list, *m_post_process_output_RT,
        RHIResourceStateType::STATE_COPY_SOURCE, RHIResourceStateType::STATE_UNORDERED_ACCESS ))

    RETURN_IF_FALSE(RHIUtils::Instance().AddRenderTargetBarrierToCommandList(command_list, *m_post_process_input_RT,
        RHIResourceStateType::STATE_NON_PIXEL_SHADER_RESOURCE, RHIResourceStateType::STATE_PIXEL_SHADER_RESOURCE))
    
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

bool glTFComputePassRayTracingPostprocess::SetupRootSignature(glTFRenderResourceManager& resource_manager)
{
    RETURN_IF_FALSE(glTFComputePassBase::SetupRootSignature(resource_manager))

    m_dispatch_count = {resource_manager.GetSwapchain().GetWidth() / 8, resource_manager.GetSwapchain().GetHeight() / 8, 1};
    
    RETURN_IF_FALSE(m_accumulation_resource.RegisterSignature(m_root_signature_helper))
    RETURN_IF_FALSE(m_custom_resource.RegisterSignature(m_root_signature_helper))

    m_post_process_input_RT = resource_manager.GetRenderTargetManager().GetRenderTargetWithTag("RayTracingOutput");
    RETURN_IF_FALSE(m_main_descriptor_heap->CreateShaderResourceViewInDescriptorHeap(resource_manager.GetDevice(), m_main_descriptor_heap->GetUsedDescriptorCount(),
            *m_post_process_input_RT, {m_post_process_input_RT->GetRenderTargetFormat(), RHIResourceDimension::TEXTURE2D}, m_post_process_input_handle))

    m_screen_uv_offset_RT = resource_manager.GetRenderTargetManager().GetRenderTargetWithTag("RayTracingScreenUVOffset");
    RETURN_IF_FALSE(m_main_descriptor_heap->CreateShaderResourceViewInDescriptorHeap(resource_manager.GetDevice(), m_main_descriptor_heap->GetUsedDescriptorCount(),
            *m_screen_uv_offset_RT, {m_screen_uv_offset_RT->GetRenderTargetFormat(), RHIResourceDimension::TEXTURE2D}, m_screen_uv_offset_handle))

    RETURN_IF_FALSE(m_main_descriptor_heap->CreateUnOrderAccessViewInDescriptorHeap(resource_manager.GetDevice(), m_main_descriptor_heap->GetUsedDescriptorCount(),
                    *m_post_process_output_RT, {m_post_process_output_RT->GetRenderTargetFormat(), RHIResourceDimension::TEXTURE2D}, m_post_process_output_handle))
    
    RETURN_IF_FALSE(m_root_signature_helper.AddTableRootParameter("PostProcessInput", RHIRootParameterDescriptorRangeType::SRV, 1, false, m_process_input_allocation))
    RETURN_IF_FALSE(m_root_signature_helper.AddTableRootParameter("ScreenUVOffset", RHIRootParameterDescriptorRangeType::SRV, 1, false, m_screen_uv_offset_allocation))
    RETURN_IF_FALSE(m_root_signature_helper.AddTableRootParameter("PostProcessOutput", RHIRootParameterDescriptorRangeType::UAV, 1, false, m_process_output_allocation))

    return true;
}

bool glTFComputePassRayTracingPostprocess::SetupPipelineStateObject(glTFRenderResourceManager& resource_manager)
{
    RETURN_IF_FALSE(glTFComputePassBase::SetupPipelineStateObject(resource_manager))

    RETURN_IF_FALSE(m_accumulation_resource.CreateDescriptors(resource_manager, *m_main_descriptor_heap))
    RETURN_IF_FALSE(m_custom_resource.CreateDescriptors(resource_manager, *m_main_descriptor_heap))

    GetComputePipelineStateObject().BindShaderCode(
            R"(glTFResources\ShaderSource\ComputeShader\PathTracingPostProcess.hlsl)", RHIShaderType::Compute, "main");
    
    auto& shader_macros = GetComputePipelineStateObject().GetShaderMacros();
    RETURN_IF_FALSE(m_accumulation_resource.AddShaderMacros(shader_macros))
    RETURN_IF_FALSE(m_custom_resource.AddShaderMacros(shader_macros))
    
    shader_macros.AddSRVRegisterDefine("POST_PROCESS_INPUT_REGISTER_INDEX", m_process_input_allocation.register_index, m_process_input_allocation.space);
    shader_macros.AddSRVRegisterDefine("SCREEN_UV_OFFSET_REGISTER_INDEX", m_screen_uv_offset_allocation.register_index, m_screen_uv_offset_allocation.space);
    shader_macros.AddUAVRegisterDefine("POST_PROCESS_OUTPUT_REGISTER_INDEX", m_process_output_allocation.register_index, m_process_output_allocation.space);
    
    return true;
}