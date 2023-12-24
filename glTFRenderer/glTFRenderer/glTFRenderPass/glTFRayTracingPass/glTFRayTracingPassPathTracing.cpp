#include "glTFRayTracingPassPathTracing.h"

#include "glTFRenderPass/glTFRenderInterface/glTFRenderInterfaceFrameStat.h"
#include "glTFRenderPass/glTFRenderInterface/glTFRenderInterfaceLighting.h"
#include "glTFRenderPass/glTFRenderInterface/glTFRenderInterfaceSceneMaterial.h"
#include "glTFRenderPass/glTFRenderInterface/glTFRenderInterfaceSceneMeshInfo.h"
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
    , m_trace_count({0, 0, 0})
    , m_material_uploaded(false)
{
    AddRenderInterface(std::make_shared<glTFRenderInterfaceFrameStat>());
    AddRenderInterface(std::make_shared<glTFRenderInterfaceSceneView>());
    AddRenderInterface(std::make_shared<glTFRenderInterfaceSceneMeshInfo>());
    AddRenderInterface(std::make_shared<glTFRenderInterfaceLighting>());
    AddRenderInterface(std::make_shared<glTFRenderInterfaceSceneMaterial>());

    m_raygen_name = "PathTracingRayGen";
    m_primary_ray_closest_hit_name = "PrimaryRayClosestHit";
    m_primary_ray_miss_name = "PrimaryRayMiss";
    m_primary_ray_hit_group_name = "MyHitGroup";
    m_shadow_ray_miss_name = "ShadowRayMiss"; 
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
    screen_uv_offset_render_target.clearValue.clear_format = RHIDataFormat::R8G8B8A8_UNORM;
    screen_uv_offset_render_target.clearValue.clear_color = {0.0f, 0.0f, 0.0f, 0.0f};
    
    m_screen_uv_offset_output = resource_manager.GetRenderTargetManager().CreateRenderTarget(
                    resource_manager.GetDevice(), RHIRenderTargetType::RTV, RHIDataFormat::R8G8B8A8_UNORM, RHIDataFormat::R8G8B8A8_UNORM, screen_uv_offset_render_target);
    resource_manager.GetRenderTargetManager().RegisterRenderTargetWithTag("RayTracingScreenUVOffset", m_screen_uv_offset_output);
    
    RETURN_IF_FALSE(glTFRayTracingPassBase::InitPass(resource_manager))
    
    RETURN_IF_FALSE(RHIUtils::Instance().AddRenderTargetBarrierToCommandList(resource_manager.GetCommandListForRecord(), *m_raytracing_output,
        RHIResourceStateType::STATE_RENDER_TARGET, RHIResourceStateType::STATE_PIXEL_SHADER_RESOURCE))
    
    RETURN_IF_FALSE(RHIUtils::Instance().AddRenderTargetBarrierToCommandList(resource_manager.GetCommandListForRecord(), *m_screen_uv_offset_output,
        RHIResourceStateType::STATE_RENDER_TARGET, RHIResourceStateType::STATE_PIXEL_SHADER_RESOURCE))

    RETURN_IF_FALSE(UpdateAS(resource_manager))
    
    m_shader_table = RHIResourceFactory::CreateRHIResource<IRHIShaderTable>();

    const auto& meshes = resource_manager.GetMeshManager().GetMeshRenderResources();
    std::vector<std::shared_ptr<RHIShaderTableRecordBase>> hit_group_records(meshes.size());
    for (const auto& mesh_info : resource_manager.GetMeshManager().GetMeshRenderResources())
    {
        hit_group_records[mesh_info.first] = std::make_shared<RHIShaderBindingTableRecordPathTracing>(mesh_info.second.material_id); 
    }
    
    std::vector<RHIShaderBindingTable> sbts;
    auto& primary_ray_sbt = sbts.emplace_back();
    primary_ray_sbt.raygen_entry = m_raygen_name;
    primary_ray_sbt.miss_entry = m_primary_ray_miss_name;
    primary_ray_sbt.hit_group_entry = m_primary_ray_hit_group_name; 
    primary_ray_sbt.hit_group_records = hit_group_records;

    auto& shadow_ray_sbt = sbts.emplace_back();
    shadow_ray_sbt.raygen_entry = m_raygen_name;
    shadow_ray_sbt.miss_entry = m_shadow_ray_miss_name;
    shadow_ray_sbt.hit_group_entry = m_shadow_ray_hit_group_name;
    
    RETURN_IF_FALSE(m_shader_table->InitShaderTable(resource_manager.GetDevice(), GetRayTracingPipelineStateObject(), *m_raytracing_as, sbts))

    RETURN_IF_FALSE(GetRenderInterface<glTFRenderInterfaceSceneMeshInfo>()->UpdateSceneMeshData(resource_manager.GetMeshManager()))
    
    return true;
}

bool glTFRayTracingPassPathTracing::PreRenderPass(glTFRenderResourceManager& resource_manager)
{
    RETURN_IF_FALSE(UpdateAS(resource_manager))
    RETURN_IF_FALSE(glTFRayTracingPassBase::PreRenderPass(resource_manager))
    
    auto& command_list = resource_manager.GetCommandListForRecord();
    RETURN_IF_FALSE(RHIUtils::Instance().SetDTToRootParameterSlot(command_list, m_output_allocation.parameter_index, m_output_handle, false))
    RETURN_IF_FALSE(RHIUtils::Instance().SetDTToRootParameterSlot(command_list, m_screen_uv_offset_allocation.parameter_index, m_screen_uv_offset_handle, false))
    
    RETURN_IF_FALSE(RHIUtils::Instance().SetSRVToRootParameterSlot(command_list, m_raytracing_as_allocation.parameter_index, m_raytracing_as->GetTLASHandle(), false)) 
    if (!m_material_uploaded)
    {
        GetRenderInterface<glTFRenderInterfaceSceneMaterial>()->UploadMaterialData(resource_manager, *m_main_descriptor_heap);
        m_material_uploaded = true;
    }

    RETURN_IF_FALSE(GetRenderInterface<glTFRenderInterfaceLighting>()->UpdateCPUBuffer())

    RETURN_IF_FALSE(RHIUtils::Instance().AddRenderTargetBarrierToCommandList(command_list, *m_raytracing_output,
            RHIResourceStateType::STATE_PIXEL_SHADER_RESOURCE, RHIResourceStateType::STATE_UNORDERED_ACCESS))

    RETURN_IF_FALSE(RHIUtils::Instance().AddRenderTargetBarrierToCommandList(command_list, *m_screen_uv_offset_output,
            RHIResourceStateType::STATE_PIXEL_SHADER_RESOURCE, RHIResourceStateType::STATE_UNORDERED_ACCESS))
    
    return true;
}

bool glTFRayTracingPassPathTracing::PostRenderPass(glTFRenderResourceManager& resource_manager)
{
    RETURN_IF_FALSE(glTFRayTracingPassBase::PostRenderPass(resource_manager))

    auto& command_list = resource_manager.GetCommandListForRecord();

    // Copy compute result to swap chain back buffer
    /*
    RETURN_IF_FALSE(RHIUtils::Instance().AddRenderTargetBarrierToCommandList(command_list, resource_manager.GetCurrentFrameSwapchainRT(),
        RHIResourceStateType::STATE_RENDER_TARGET, RHIResourceStateType::STATE_COPY_DEST))

    RETURN_IF_FALSE(RHIUtils::Instance().AddRenderTargetBarrierToCommandList(command_list, *m_raytracing_output,
            RHIResourceStateType::STATE_UNORDERED_ACCESS, RHIResourceStateType::STATE_COPY_SOURCE ))
    
    RETURN_IF_FALSE(RHIUtils::Instance().CopyTexture(command_list, resource_manager.GetCurrentFrameSwapchainRT(), *m_raytracing_output))

    RETURN_IF_FALSE(RHIUtils::Instance().AddRenderTargetBarrierToCommandList(command_list, resource_manager.GetCurrentFrameSwapchainRT(),
        RHIResourceStateType::STATE_COPY_DEST, RHIResourceStateType::STATE_RENDER_TARGET))

    RETURN_IF_FALSE(RHIUtils::Instance().AddRenderTargetBarrierToCommandList(command_list, *m_raytracing_output,
        RHIResourceStateType::STATE_COPY_SOURCE, RHIResourceStateType::STATE_UNORDERED_ACCESS ))
    */
    
    RETURN_IF_FALSE(RHIUtils::Instance().AddRenderTargetBarrierToCommandList(command_list, *m_raytracing_output,
        RHIResourceStateType::STATE_UNORDERED_ACCESS, RHIResourceStateType::STATE_PIXEL_SHADER_RESOURCE))

    RETURN_IF_FALSE(RHIUtils::Instance().AddRenderTargetBarrierToCommandList(command_list, *m_screen_uv_offset_output,
        RHIResourceStateType::STATE_UNORDERED_ACCESS, RHIResourceStateType::STATE_PIXEL_SHADER_RESOURCE))
    
    return true;
}

bool glTFRayTracingPassPathTracing::TryProcessSceneObject(glTFRenderResourceManager& resource_manager,
    const glTFSceneObjectBase& object)
{
    const glTFLightBase* light = dynamic_cast<const glTFLightBase*>(&object);
    if (!light)
    {
        return false;
    }
    
    return GetRenderInterface<glTFRenderInterfaceLighting>()->UpdateLightInfo(*light);
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
    
    RETURN_IF_FALSE(glTFRayTracingPassBase::SetupRootSignature(resource_manager))
    
    m_trace_count = {resource_manager.GetSwapchain().GetWidth(), resource_manager.GetSwapchain().GetHeight(), 1};

    return true;
}

bool glTFRayTracingPassPathTracing::SetupPipelineStateObject(glTFRenderResourceManager& resource_manager)
{
    RETURN_IF_FALSE(glTFRayTracingPassBase::SetupPipelineStateObject(resource_manager))

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
    GetRayTracingPipelineStateObject().AddHitGroupDesc({m_primary_ray_hit_group_name,
        m_primary_ray_closest_hit_name,
        "",
        ""
    });

    // Local RS setup
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

    IRHIRayTracingPipelineStateObject::RHIRayTracingConfig config;
    config.payload_size = sizeof(float) * 10;
    config.attribute_size = sizeof(float) * 2;
    config.max_recursion_count = 1;
    GetRayTracingPipelineStateObject().SetConfig(config);

    GetRayTracingPipelineStateObject().SetExportFunctionNames(
        {
            m_raygen_name,
            m_primary_ray_closest_hit_name,
            m_primary_ray_miss_name,
            m_shadow_ray_miss_name,
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
    RETURN_IF_FALSE(m_raytracing_as->InitRayTracingAS(resource_manager.GetDevice(), resource_manager.GetCommandListForRecord(), resource_manager.GetMeshManager()))
    
    return true;
}
