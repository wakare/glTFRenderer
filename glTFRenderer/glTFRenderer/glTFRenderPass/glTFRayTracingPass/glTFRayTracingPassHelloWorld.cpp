#include "glTFRayTracingPassHelloWorld.h"

#include "glTFRHI/RHIResourceFactoryImpl.hpp"
#include "glTFRHI/RHIUtils.h"
#include "glTFRHI/RHIInterface/IRHIRenderTargetManager.h"

glTFRayTracingPassHelloWorld::glTFRayTracingPassHelloWorld()
    : m_raytracing_output(nullptr)
    , m_trace_count({0, 0, 0})
{
}

const char* glTFRayTracingPassHelloWorld::PassName()
{
    return "RayTracingPass_HelloWorld";
}

bool glTFRayTracingPassHelloWorld::InitPass(glTFRenderResourceManager& resource_manager)
{
    IRHIRenderTargetDesc raytracing_output_render_target;
    raytracing_output_render_target.width = resource_manager.GetSwapchain().GetWidth();
    raytracing_output_render_target.height = resource_manager.GetSwapchain().GetHeight();
    raytracing_output_render_target.name = "RayTracingOutput";
    raytracing_output_render_target.isUAV = true;
    raytracing_output_render_target.clearValue.clear_format = RHIDataFormat::R8G8B8A8_UNORM;
    raytracing_output_render_target.clearValue.clear_color = {0.0f, 0.0f, 0.0f, 0.0f};

    m_raytracing_output = resource_manager.GetRenderTargetManager().CreateRenderTarget(
                resource_manager.GetDevice(), RHIRenderTargetType::RTV, RHIDataFormat::R8G8B8A8_UNORM, RHIDataFormat::R8G8B8A8_UNORM, raytracing_output_render_target);
    
    RETURN_IF_FALSE(glTFRayTracingPassBase::InitPass(resource_manager))
    
    auto& command_list = resource_manager.GetCommandListForRecord();

    RETURN_IF_FALSE(RHIUtils::Instance().AddRenderTargetBarrierToCommandList(command_list, *m_raytracing_output,
        RHIResourceStateType::COMMON, RHIResourceStateType::UNORDER_ACCESS))

    m_shader_table = RHIResourceFactory::CreateRHIResource<IRHIShaderTable>();
    RETURN_IF_FALSE(m_shader_table->InitShaderTable(resource_manager.GetDevice(), GetRayTracingPipelineStateObject()))
    
    return true;
}

bool glTFRayTracingPassHelloWorld::PreRenderPass(glTFRenderResourceManager& resource_manager)
{
    RETURN_IF_FALSE(glTFRayTracingPassBase::PreRenderPass(resource_manager))

    return true;
}

bool glTFRayTracingPassHelloWorld::PostRenderPass(glTFRenderResourceManager& resource_manager)
{
    RETURN_IF_FALSE(glTFRayTracingPassBase::PostRenderPass(resource_manager))

    auto& command_list = resource_manager.GetCommandListForRecord();

    // Copy compute result to swapchain back buffer
    RETURN_IF_FALSE(RHIUtils::Instance().AddRenderTargetBarrierToCommandList(command_list, resource_manager.GetCurrentFrameSwapchainRT(),
        RHIResourceStateType::RENDER_TARGET, RHIResourceStateType::COPY_DEST))

    RETURN_IF_FALSE(RHIUtils::Instance().AddRenderTargetBarrierToCommandList(command_list, *m_raytracing_output,
            RHIResourceStateType::UNORDER_ACCESS, RHIResourceStateType::COPY_SOURCE ))
    
    RETURN_IF_FALSE(RHIUtils::Instance().CopyTexture(command_list, resource_manager.GetCurrentFrameSwapchainRT(), *m_raytracing_output))

    // Copy compute result to swapchain back buffer
    RETURN_IF_FALSE(RHIUtils::Instance().AddRenderTargetBarrierToCommandList(command_list, resource_manager.GetCurrentFrameSwapchainRT(),
        RHIResourceStateType::COPY_DEST, RHIResourceStateType::RENDER_TARGET))

    RETURN_IF_FALSE(RHIUtils::Instance().AddRenderTargetBarrierToCommandList(command_list, *m_raytracing_output,
        RHIResourceStateType::COPY_SOURCE, RHIResourceStateType::UNORDER_ACCESS ))
    
    return true;
}

IRHIShaderTable& glTFRayTracingPassHelloWorld::GetShaderTable() const
{
    return *m_shader_table;
}

TraceCount glTFRayTracingPassHelloWorld::GetTraceCount() const
{
    return m_trace_count;
}

size_t glTFRayTracingPassHelloWorld::GetRootSignatureParameterCount()
{
    return 1;
}

size_t glTFRayTracingPassHelloWorld::GetRootSignatureSamplerCount()
{
    return 0;
}

size_t glTFRayTracingPassHelloWorld::GetMainDescriptorHeapSize()
{
    return 64;
}

bool glTFRayTracingPassHelloWorld::SetupRootSignature(glTFRenderResourceManager& resource_manager)
{
    RETURN_IF_FALSE(glTFRayTracingPassBase::SetupRootSignature(resource_manager))

    m_trace_count = {resource_manager.GetSwapchain().GetWidth(), resource_manager.GetSwapchain().GetHeight(), 1};

    const RHIRootParameterDescriptorRangeDesc UAVRangeDesc {RHIRootParameterDescriptorRangeType::UAV, 0, 1};
    m_root_signature->GetRootParameter(0).InitAsDescriptorTableRange(1, &UAVRangeDesc);
    
    return true;
}

bool glTFRayTracingPassHelloWorld::SetupPipelineStateObject(glTFRenderResourceManager& resource_manager)
{
    RETURN_IF_FALSE(glTFRayTracingPassBase::SetupPipelineStateObject(resource_manager))

    GetRayTracingPipelineStateObject().BindShaderCode("glTFResources/ShaderSource/RayTracing/RayTracingHelloWorld.hlsl", RHIShaderType::RayTracing, "");

    RHIGPUDescriptorHandle UAV;
    RETURN_IF_FALSE(m_main_descriptor_heap->CreateUnOrderAccessViewInDescriptorHeap(resource_manager.GetDevice(), m_main_descriptor_heap->GetUsedDescriptorCount(),
                *m_raytracing_output, {m_raytracing_output->GetRenderTargetFormat(), RHIResourceDimension::TEXTURE2D}, UAV))
    
    return true;
}

bool glTFRayTracingPassHelloWorld::BuildAS(glTFRenderResourceManager& resource_manager)
{

    
    return true;
}
