#include "glTFRayTracingPassPathTracing.h"

#include "glTFRenderPass/glTFRenderInterface/glTFRenderInterfaceSceneMaterial.h"
#include "glTFRHI/RHIResourceFactoryImpl.hpp"
#include "glTFRHI/RHIUtils.h"
#include "glTFRHI/RHIInterface/IRHIRenderTargetManager.h"

const char* hit_group_name = "MyHitGroup";

glTFRayTracingPassPathTracing::glTFRayTracingPassPathTracing()
    : m_raytracing_output(nullptr)
    , m_trace_count({0, 0, 0})
    , m_material_uploaded(false)
{
    AddRenderInterface(std::make_shared<glTFRenderInterfaceSceneView>());
    AddRenderInterface(std::make_shared<glTFRenderInterfaceSceneMaterial>());
}

const char* glTFRayTracingPassPathTracing::PassName()
{
    return "RayTracingPass_PathTracing";
}

bool glTFRayTracingPassPathTracing::InitPass(glTFRenderResourceManager& resource_manager)
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
    
    RETURN_IF_FALSE(RHIUtils::Instance().AddRenderTargetBarrierToCommandList(resource_manager.GetCommandListForRecord(), *m_raytracing_output,
        RHIResourceStateType::STATE_RENDER_TARGET, RHIResourceStateType::STATE_UNORDERED_ACCESS))

    RETURN_IF_FALSE(UpdateAS(resource_manager))
    
    m_shader_table = RHIResourceFactory::CreateRHIResource<IRHIShaderTable>();
    const auto& bind_rt_shader = GetRayTracingPipelineStateObject().GetBindShader(RHIShaderType::RayTracing);

    std::vector<RHIShaderBindingTable> sbts;
    sbts.push_back({bind_rt_shader.GetRayTracingEntryFunctionNames().raygen_shader_entry_name,
        bind_rt_shader.GetRayTracingEntryFunctionNames().miss_shader_entry_name,
        hit_group_name,
        nullptr, nullptr, nullptr});
    
    RETURN_IF_FALSE(m_shader_table->InitShaderTable(resource_manager.GetDevice(), GetRayTracingPipelineStateObject(), *m_raytracing_as, sbts))
    
    return true;
}

bool glTFRayTracingPassPathTracing::PreRenderPass(glTFRenderResourceManager& resource_manager)
{
    RETURN_IF_FALSE(UpdateAS(resource_manager))
    RETURN_IF_FALSE(glTFRayTracingPassBase::PreRenderPass(resource_manager))
    
    auto& command_list = resource_manager.GetCommandListForRecord();
    RETURN_IF_FALSE(RHIUtils::Instance().SetDTToRootParameterSlot(command_list, m_output_allocation.parameter_index, m_main_descriptor_heap->GetGPUHandle(0),false))    
    RETURN_IF_FALSE(RHIUtils::Instance().SetSRVToRootParameterSlot(command_list, m_raytracing_as_allocation.parameter_index, m_raytracing_as->GetTLASHandle(), false)) 
    if (!m_material_uploaded)
    {
        GetRenderInterface<glTFRenderInterfaceSceneMaterial>()->UploadMaterialData(resource_manager, *m_main_descriptor_heap);
        m_material_uploaded = true;
    }
    return true;
}

bool glTFRayTracingPassPathTracing::PostRenderPass(glTFRenderResourceManager& resource_manager)
{
    RETURN_IF_FALSE(glTFRayTracingPassBase::PostRenderPass(resource_manager))

    auto& command_list = resource_manager.GetCommandListForRecord();

    // Copy compute result to swapchain back buffer
    RETURN_IF_FALSE(RHIUtils::Instance().AddRenderTargetBarrierToCommandList(command_list, resource_manager.GetCurrentFrameSwapchainRT(),
        RHIResourceStateType::STATE_RENDER_TARGET, RHIResourceStateType::STATE_COPY_DEST))

    RETURN_IF_FALSE(RHIUtils::Instance().AddRenderTargetBarrierToCommandList(command_list, *m_raytracing_output,
            RHIResourceStateType::STATE_UNORDERED_ACCESS, RHIResourceStateType::STATE_COPY_SOURCE ))
    
    RETURN_IF_FALSE(RHIUtils::Instance().CopyTexture(command_list, resource_manager.GetCurrentFrameSwapchainRT(), *m_raytracing_output))

    RETURN_IF_FALSE(RHIUtils::Instance().AddRenderTargetBarrierToCommandList(command_list, resource_manager.GetCurrentFrameSwapchainRT(),
        RHIResourceStateType::STATE_COPY_DEST, RHIResourceStateType::STATE_RENDER_TARGET))

    RETURN_IF_FALSE(RHIUtils::Instance().AddRenderTargetBarrierToCommandList(command_list, *m_raytracing_output,
        RHIResourceStateType::STATE_COPY_SOURCE, RHIResourceStateType::STATE_UNORDERED_ACCESS ))
    
    return true;
}

IRHIShaderTable& glTFRayTracingPassPathTracing::GetShaderTable() const
{
    return *m_shader_table;
}

TraceCount glTFRayTracingPassPathTracing::GetTraceCount() const
{
    return m_trace_count;
}

size_t glTFRayTracingPassPathTracing::GetMainDescriptorHeapSize()
{
    return 512;
}

bool glTFRayTracingPassPathTracing::SetupRootSignature(glTFRenderResourceManager& resource_manager)
{
    // Non-bindless table parameter should be added first
    RETURN_IF_FALSE(m_root_signature_helper.AddTableRootParameter("output", RHIRootParameterDescriptorRangeType::UAV, 1, false, m_output_allocation))
    RETURN_IF_FALSE(m_root_signature_helper.AddSRVRootParameter("RaytracingAS", m_raytracing_as_allocation))
    
    RETURN_IF_FALSE(glTFRayTracingPassBase::SetupRootSignature(resource_manager))
    
    m_trace_count = {resource_manager.GetSwapchain().GetWidth(), resource_manager.GetSwapchain().GetHeight(), 1};

    return true;
}

bool glTFRayTracingPassPathTracing::SetupPipelineStateObject(glTFRenderResourceManager& resource_manager)
{
    RETURN_IF_FALSE(glTFRayTracingPassBase::SetupPipelineStateObject(resource_manager))

    RHIGPUDescriptorHandle UAV;
    RETURN_IF_FALSE(m_main_descriptor_heap->CreateUnOrderAccessViewInDescriptorHeap(resource_manager.GetDevice(), m_main_descriptor_heap->GetUsedDescriptorCount(),
                *m_raytracing_output, {m_raytracing_output->GetRenderTargetFormat(), RHIResourceDimension::TEXTURE2D}, UAV))
    
    GetRayTracingPipelineStateObject().BindShaderCode("glTFResources/ShaderSource/RayTracing/RayTracingHelloWorld.hlsl",
        RHIShaderType::RayTracing, "",
        {
            "MyRaygenShader",
            "MyMissShader",
            "MyClosestHitShader",
            "",
            ""
        });
    
    auto& shader_macros = GetRayTracingPipelineStateObject().GetShaderMacros();
    shader_macros.AddSRVRegisterDefine("SCENE_AS_REGISTER_INDEX", m_raytracing_as_allocation.register_index);
    shader_macros.AddUAVRegisterDefine("OUTPUT_REGISTER_INDEX", m_output_allocation.register_index);

    const auto& bind_rt_shader = GetRayTracingPipelineStateObject().GetBindShader(RHIShaderType::RayTracing);
    
    // One ray type mapping one hit group desc
    GetRayTracingPipelineStateObject().AddHitGroupDesc({hit_group_name,
        bind_rt_shader.GetRayTracingEntryFunctionNames().closest_hit_shader_entry_name,
        bind_rt_shader.GetRayTracingEntryFunctionNames().any_hit_shader_entry_name,
        bind_rt_shader.GetRayTracingEntryFunctionNames().intersection_shader_entry_name
    });
    
    return true;
}

bool glTFRayTracingPassPathTracing::UpdateAS(glTFRenderResourceManager& resource_manager)
{
    if (!m_raytracing_as)
    {
        RETURN_IF_FALSE(BuildAS(resource_manager))
    }

    return true;
}

bool glTFRayTracingPassPathTracing::BuildAS(glTFRenderResourceManager& resource_manager)
{
    m_raytracing_as = RHIResourceFactory::CreateRHIResource<IRHIRayTracingAS>();
    RETURN_IF_FALSE(m_raytracing_as->InitRayTracingAS(resource_manager.GetDevice(), resource_manager.GetCommandListForRecord(), resource_manager.GetMeshManager().GetMeshes()))
    
    return true;
}
