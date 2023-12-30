#include "glTFRayTracingPassPathTracing.h"
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
    , m_trace_count({0, 0, 0})
    , m_material_uploaded(false)
{
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
    resource_manager.GetRenderTargetManager().RegisterRenderTargetWithTag("RayTracingOutput", m_raytracing_output);
    
    IRHIRenderTargetDesc screen_uv_offset_render_target;
    screen_uv_offset_render_target.width = resource_manager.GetSwapchain().GetWidth();
    screen_uv_offset_render_target.height = resource_manager.GetSwapchain().GetHeight();
    screen_uv_offset_render_target.name = "ScreenUVOffset";
    screen_uv_offset_render_target.isUAV = true;
    screen_uv_offset_render_target.clearValue.clear_format = RHIDataFormat::R32G32B32A32_FLOAT;
    screen_uv_offset_render_target.clearValue.clear_color = {0.0f, 0.0f, 0.0f, 0.0f};
    
    m_screen_uv_offset_output = resource_manager.GetRenderTargetManager().CreateRenderTarget(
                    resource_manager.GetDevice(), RHIRenderTargetType::RTV, RHIDataFormat::R32G32B32A32_FLOAT, RHIDataFormat::R32G32B32A32_FLOAT, screen_uv_offset_render_target);
    resource_manager.GetRenderTargetManager().RegisterRenderTargetWithTag("RayTracingScreenUVOffset", m_screen_uv_offset_output);
    
    RETURN_IF_FALSE(glTFRayTracingPassWithMesh::InitPass(resource_manager))
    
    RETURN_IF_FALSE(RHIUtils::Instance().AddRenderTargetBarrierToCommandList(resource_manager.GetCommandListForRecord(), *m_raytracing_output,
        RHIResourceStateType::STATE_RENDER_TARGET, RHIResourceStateType::STATE_PIXEL_SHADER_RESOURCE))
    
    RETURN_IF_FALSE(RHIUtils::Instance().AddRenderTargetBarrierToCommandList(resource_manager.GetCommandListForRecord(), *m_screen_uv_offset_output,
        RHIResourceStateType::STATE_RENDER_TARGET, RHIResourceStateType::STATE_PIXEL_SHADER_RESOURCE))

    m_shader_table = RHIResourceFactory::CreateRHIResource<IRHIShaderTable>();

    std::vector<RHIShaderBindingTable> sbts;
    auto& primary_ray_sbt = sbts.emplace_back();
    primary_ray_sbt.raygen_entry = GetRayGenFunctionName();
    primary_ray_sbt.miss_entry = GetPrimaryRayMissFunctionName();
    primary_ray_sbt.hit_group_entry = GetPrimaryRayHitGroupName();

    auto& shadow_ray_sbt = sbts.emplace_back();
    shadow_ray_sbt.raygen_entry = GetRayGenFunctionName();
    shadow_ray_sbt.miss_entry = GetShadowRayMissFunctionName();
    shadow_ray_sbt.hit_group_entry = GetShadowRayHitGroupName();
    
    RETURN_IF_FALSE(m_shader_table->InitShaderTable(resource_manager.GetDevice(), GetRayTracingPipelineStateObject(), *m_raytracing_as, sbts))

    return true;
}

bool glTFRayTracingPassPathTracing::PreRenderPass(glTFRenderResourceManager& resource_manager)
{
    RETURN_IF_FALSE(glTFRayTracingPassWithMesh::PreRenderPass(resource_manager))
    
    auto& command_list = resource_manager.GetCommandListForRecord();
    RETURN_IF_FALSE(RHIUtils::Instance().SetDTToRootParameterSlot(command_list, m_output_allocation.parameter_index, m_output_handle, false))
    RETURN_IF_FALSE(RHIUtils::Instance().SetDTToRootParameterSlot(command_list, m_screen_uv_offset_allocation.parameter_index, m_screen_uv_offset_handle, false))
    
    RETURN_IF_FALSE(RHIUtils::Instance().SetSRVToRootParameterSlot(command_list, m_raytracing_as_allocation.parameter_index, m_raytracing_as->GetTLASHandle(), false)) 
    /*if (!m_material_uploaded)
    {
        GetRenderInterface<glTFRenderInterfaceSceneMaterial>()->UploadMaterialData(resource_manager, *m_main_descriptor_heap);
        m_material_uploaded = true;
    }
    */

    RETURN_IF_FALSE(RHIUtils::Instance().AddRenderTargetBarrierToCommandList(command_list, *m_raytracing_output,
            RHIResourceStateType::STATE_PIXEL_SHADER_RESOURCE, RHIResourceStateType::STATE_UNORDERED_ACCESS))

    RETURN_IF_FALSE(RHIUtils::Instance().AddRenderTargetBarrierToCommandList(command_list, *m_screen_uv_offset_output,
            RHIResourceStateType::STATE_PIXEL_SHADER_RESOURCE, RHIResourceStateType::STATE_UNORDERED_ACCESS))
    
    return true;
}

bool glTFRayTracingPassPathTracing::PostRenderPass(glTFRenderResourceManager& resource_manager)
{
    RETURN_IF_FALSE(glTFRayTracingPassWithMesh::PostRenderPass(resource_manager))

    auto& command_list = resource_manager.GetCommandListForRecord();

    RETURN_IF_FALSE(RHIUtils::Instance().AddRenderTargetBarrierToCommandList(command_list, *m_raytracing_output,
        RHIResourceStateType::STATE_UNORDERED_ACCESS, RHIResourceStateType::STATE_PIXEL_SHADER_RESOURCE))

    RETURN_IF_FALSE(RHIUtils::Instance().AddRenderTargetBarrierToCommandList(command_list, *m_screen_uv_offset_output,
        RHIResourceStateType::STATE_UNORDERED_ACCESS, RHIResourceStateType::STATE_PIXEL_SHADER_RESOURCE))
    
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
    RETURN_IF_FALSE(m_root_signature_helper.AddTableRootParameter("screen_uv_offset", RHIRootParameterDescriptorRangeType::UAV, 1, false, m_screen_uv_offset_allocation))
    RETURN_IF_FALSE(m_root_signature_helper.AddSRVRootParameter("RaytracingAS", m_raytracing_as_allocation))
    
    RETURN_IF_FALSE(glTFRayTracingPassWithMesh::SetupRootSignature(resource_manager))
    
    m_trace_count = {resource_manager.GetSwapchain().GetWidth(), resource_manager.GetSwapchain().GetHeight(), 1};

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
    shader_macros.AddSRVRegisterDefine("SCENE_AS_REGISTER_INDEX", m_raytracing_as_allocation.register_index, m_raytracing_as_allocation.space);
    shader_macros.AddUAVRegisterDefine("OUTPUT_REGISTER_INDEX", m_output_allocation.register_index, m_output_allocation.space);
    shader_macros.AddUAVRegisterDefine("SCREEN_UV_OFFSET_REGISTER_INDEX", m_screen_uv_offset_allocation.register_index, m_screen_uv_offset_allocation.space);

    // One ray type mapping one hit group desc
    GetRayTracingPipelineStateObject().AddHitGroupDesc({GetPrimaryRayHitGroupName(),
        GetPrimaryRayCHFunctionName(),
        "",
        ""
    });

    // Local RS setup
    /*
    m_local_rs.SetRegisterSpace(1);
    m_local_rs.SetUsage(RHIRootSignatureUsage::LocalRS);
    m_local_rs.AddConstantRootParameter("LocalRS_Constant", 1, m_local_constant_allocation);
    m_local_rs.BuildRootSignature(resource_manager.GetDevice());

    shader_macros.AddCBVRegisterDefine("MATERIAL_ID_REGISTER_INDEX", m_local_constant_allocation.register_index, m_local_rs.GetRegisterSpace());
    
    GetRayTracingPipelineStateObject().AddLocalRSDesc({
        m_local_rs.GetRootSignaturePointer(),
        {
            m_primary_ray_closest_hit_name
        }
    });
    */

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
