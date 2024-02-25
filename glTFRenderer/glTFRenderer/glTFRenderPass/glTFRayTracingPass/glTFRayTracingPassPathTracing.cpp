#include "glTFRayTracingPassPathTracing.h"

#include <imgui.h>

#include "glTFRenderPass/glTFRenderInterface/glTFRenderInterfaceSingleConstantBuffer.h"
#include "glTFRHI/RHIResourceFactoryImpl.hpp"
#include "glTFRHI/RHIUtils.h"
#include "glTFRHI/RHIInterface/IRHIRenderTargetManager.h"

struct RHIShaderBindingTableRecordPathTracing : RHIShaderTableRecordBase
{
    RHIShaderBindingTableRecordPathTracing(unsigned material_id)
        : m_material_id(material_id)
    {
    }

    virtual void* GetData() override
    {
        return &m_material_id;   
    }
    virtual size_t GetSize() override
    {
        return 32;
    }

    unsigned m_material_id;
};

glTFRayTracingPassPathTracing::glTFRayTracingPassPathTracing()
    : m_raytracing_output(nullptr)
    , m_screen_uv_offset_output(nullptr)
    , m_output_handle(0)
    , m_screen_uv_offset_handle(0)
{
    AddRenderInterface(std::make_shared<glTFRenderInterfaceSingleConstantBuffer<RayTracingPathTracingPassOptions>>());
}

const char* glTFRayTracingPassPathTracing::PassName()
{
    return "PathTracingPass";
}

bool glTFRayTracingPassPathTracing::InitPass(glTFRenderResourceManager& resource_manager)
{
    RHIRenderTargetDesc raytracing_output_render_target;
    raytracing_output_render_target.width = resource_manager.GetSwapChain().GetWidth();
    raytracing_output_render_target.height = resource_manager.GetSwapChain().GetHeight();
    raytracing_output_render_target.name = "RayTracingOutput";
    raytracing_output_render_target.isUAV = true;
    raytracing_output_render_target.clearValue.clear_format = RHIDataFormat::R8G8B8A8_UNORM;
    raytracing_output_render_target.clearValue.clear_color = {0.0f, 0.0f, 0.0f, 0.0f};

    m_raytracing_output = resource_manager.GetRenderTargetManager().CreateRenderTarget(
                resource_manager.GetDevice(), RHIRenderTargetType::RTV, RHIDataFormat::R8G8B8A8_UNORM, RHIDataFormat::R8G8B8A8_UNORM, raytracing_output_render_target);
    resource_manager.GetRenderTargetManager().RegisterRenderTargetWithTag("RayTracingOutput", m_raytracing_output);
    
    RHIRenderTargetDesc screen_uv_offset_render_target;
    screen_uv_offset_render_target.width = resource_manager.GetSwapChain().GetWidth();
    screen_uv_offset_render_target.height = resource_manager.GetSwapChain().GetHeight();
    screen_uv_offset_render_target.name = "ScreenUVOffset";
    screen_uv_offset_render_target.isUAV = true;
    screen_uv_offset_render_target.clearValue.clear_format = RHIDataFormat::R32G32B32A32_FLOAT;
    screen_uv_offset_render_target.clearValue.clear_color = {0.0f, 0.0f, 0.0f, 0.0f};
    
    m_screen_uv_offset_output = resource_manager.GetRenderTargetManager().CreateRenderTarget(
                    resource_manager.GetDevice(), RHIRenderTargetType::RTV, RHIDataFormat::R32G32B32A32_FLOAT, RHIDataFormat::R32G32B32A32_FLOAT, screen_uv_offset_render_target);
    resource_manager.GetRenderTargetManager().RegisterRenderTargetWithTag("RayTracingScreenUVOffset", m_screen_uv_offset_output);
    
    RETURN_IF_FALSE(RHIUtils::Instance().AddRenderTargetBarrierToCommandList(resource_manager.GetCommandListForRecord(), *m_raytracing_output,
        RHIResourceStateType::STATE_RENDER_TARGET, RHIResourceStateType::STATE_NON_PIXEL_SHADER_RESOURCE))
    
    RETURN_IF_FALSE(RHIUtils::Instance().AddRenderTargetBarrierToCommandList(resource_manager.GetCommandListForRecord(), *m_screen_uv_offset_output,
        RHIResourceStateType::STATE_RENDER_TARGET, RHIResourceStateType::STATE_NON_PIXEL_SHADER_RESOURCE))
    
    RETURN_IF_FALSE(glTFRayTracingPassWithMesh::InitPass(resource_manager))
    
    return true;
}

bool glTFRayTracingPassPathTracing::PreRenderPass(glTFRenderResourceManager& resource_manager)
{
    RETURN_IF_FALSE(glTFRayTracingPassWithMesh::PreRenderPass(resource_manager))
    
    auto& command_list = resource_manager.GetCommandListForRecord();
    RETURN_IF_FALSE(RHIUtils::Instance().SetDTToRootParameterSlot(command_list, m_output_allocation.parameter_index, m_output_handle, false))
    RETURN_IF_FALSE(RHIUtils::Instance().SetDTToRootParameterSlot(command_list, m_screen_uv_offset_allocation.parameter_index, m_screen_uv_offset_handle, false))
    
    RETURN_IF_FALSE(RHIUtils::Instance().AddRenderTargetBarrierToCommandList(command_list, *m_raytracing_output,
            RHIResourceStateType::STATE_NON_PIXEL_SHADER_RESOURCE, RHIResourceStateType::STATE_UNORDERED_ACCESS))

    RETURN_IF_FALSE(RHIUtils::Instance().AddRenderTargetBarrierToCommandList(command_list, *m_screen_uv_offset_output,
            RHIResourceStateType::STATE_NON_PIXEL_SHADER_RESOURCE, RHIResourceStateType::STATE_UNORDERED_ACCESS))

    RETURN_IF_FALSE(GetRenderInterface<glTFRenderInterfaceSingleConstantBuffer<RayTracingPathTracingPassOptions>>()->UploadCPUBuffer(&m_pass_options, 0, sizeof(m_pass_options)))
    
    return true;
}

bool glTFRayTracingPassPathTracing::PostRenderPass(glTFRenderResourceManager& resource_manager)
{
    RETURN_IF_FALSE(glTFRayTracingPassWithMesh::PostRenderPass(resource_manager))

    auto& command_list = resource_manager.GetCommandListForRecord();

    RETURN_IF_FALSE(RHIUtils::Instance().AddRenderTargetBarrierToCommandList(command_list, *m_raytracing_output,
        RHIResourceStateType::STATE_UNORDERED_ACCESS, RHIResourceStateType::STATE_NON_PIXEL_SHADER_RESOURCE))

    RETURN_IF_FALSE(RHIUtils::Instance().AddRenderTargetBarrierToCommandList(command_list, *m_screen_uv_offset_output,
        RHIResourceStateType::STATE_UNORDERED_ACCESS, RHIResourceStateType::STATE_NON_PIXEL_SHADER_RESOURCE))
    
    return true;
}

bool glTFRayTracingPassPathTracing::SetupRootSignature(glTFRenderResourceManager& resource_manager)
{
    // Non-bindless table parameter should be added first
    RETURN_IF_FALSE(m_root_signature_helper.AddTableRootParameter("output", RHIRootParameterDescriptorRangeType::UAV, 1, false, m_output_allocation))
    RETURN_IF_FALSE(m_root_signature_helper.AddTableRootParameter("screen_uv_offset", RHIRootParameterDescriptorRangeType::UAV, 1, false, m_screen_uv_offset_allocation))
    
    RETURN_IF_FALSE(glTFRayTracingPassWithMesh::SetupRootSignature(resource_manager))
    
    return true;
}

bool glTFRayTracingPassPathTracing::SetupPipelineStateObject(glTFRenderResourceManager& resource_manager)
{
    RETURN_IF_FALSE(glTFRayTracingPassWithMesh::SetupPipelineStateObject(resource_manager))

    RETURN_IF_FALSE(m_main_descriptor_heap->CreateUnOrderAccessViewInDescriptorHeap(
        resource_manager.GetDevice(),
        m_main_descriptor_heap->GetUsedDescriptorCount(),
        *m_raytracing_output,
        {
            m_raytracing_output->GetRenderTargetFormat(),
            RHIResourceDimension::TEXTURE2D
        },
        m_output_handle))

    RETURN_IF_FALSE(m_main_descriptor_heap->CreateUnOrderAccessViewInDescriptorHeap(
        resource_manager.GetDevice(),
        m_main_descriptor_heap->GetUsedDescriptorCount(),
        *m_screen_uv_offset_output,
        {
            m_screen_uv_offset_output->GetRenderTargetFormat(),
            RHIResourceDimension::TEXTURE2D
        },
        m_screen_uv_offset_handle))

    GetRayTracingPipelineStateObject().BindShaderCode("glTFResources/ShaderSource/RayTracing/PathTracingMain.hlsl",
        RHIShaderType::RayTracing, "");
    
    auto& shader_macros = GetRayTracingPipelineStateObject().GetShaderMacros();
    shader_macros.AddUAVRegisterDefine("OUTPUT_REGISTER_INDEX", m_output_allocation.register_index, m_output_allocation.space);
    shader_macros.AddUAVRegisterDefine("SCREEN_UV_OFFSET_REGISTER_INDEX", m_screen_uv_offset_allocation.register_index, m_screen_uv_offset_allocation.space);

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

const char* glTFRayTracingPassPathTracing::GetRayGenFunctionName()
{
    return "PathTracingRayGen";
}

const char* glTFRayTracingPassPathTracing::GetPrimaryRayCHFunctionName()
{
    return "PrimaryRayClosestHit";
}

const char* glTFRayTracingPassPathTracing::GetPrimaryRayMissFunctionName()
{
    return "PrimaryRayMiss";
}

const char* glTFRayTracingPassPathTracing::GetPrimaryRayHitGroupName()
{
    return "MyHitGroup";
}

const char* glTFRayTracingPassPathTracing::GetShadowRayMissFunctionName()
{
    return "ShadowRayMiss";
}

const char* glTFRayTracingPassPathTracing::GetShadowRayHitGroupName()
{
    return "ShadowHitGroup";
}

bool glTFRayTracingPassPathTracing::UpdateGUIWidgets()
{
    RETURN_IF_FALSE(glTFRayTracingPassWithMesh::UpdateGUIWidgets())

    ImGui::SliderInt("MaxBounceCount", &m_pass_options.max_bounce_count, 1, 64);
    ImGui::SliderInt("CandidateLightCount", &m_pass_options.candidate_light_count, 4, 32);
    ImGui::SliderInt("SamplesPerPixel", &m_pass_options.samples_per_pixel, 1, 64);
    ImGui::Checkbox("CheckVisibilityForAllCandidates", (bool*)&m_pass_options.check_visibility_for_all_candidates);
    ImGui::Checkbox("RISLightSampling", (bool*)&m_pass_options.ris_light_sampling);
    
    return true;
}
