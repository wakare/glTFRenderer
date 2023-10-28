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

RWTextureResourceWithBackBuffer::RWTextureResourceWithBackBuffer(std::string output_register_name,
                                                                 std::string back_register_name)
    : m_output_register_name(std::move(output_register_name))
    , m_back_register_name(std::move(back_register_name))
    , m_texture_desc()
    , m_writable_buffer_handle(0)
    , m_back_buffer_handle(0)
{
}

bool RWTextureResourceWithBackBuffer::CreateResource(glTFRenderResourceManager& resource_manager, const IRHIRenderTargetDesc& desc)
{
    m_texture_desc = desc;
    auto format = m_texture_desc.clearValue.clear_format;
    
    m_writable_buffer = resource_manager.GetRenderTargetManager().CreateRenderTarget(
                    resource_manager.GetDevice(), RHIRenderTargetType::RTV, format, format, m_texture_desc);

    auto back_buffer_format = m_texture_desc;
    back_buffer_format.isUAV = false;

    m_back_buffer = resource_manager.GetRenderTargetManager().CreateRenderTarget(
                    resource_manager.GetDevice(), RHIRenderTargetType::RTV, format, format, back_buffer_format);

    RETURN_IF_FALSE(RHIUtils::Instance().AddRenderTargetBarrierToCommandList(resource_manager.GetCommandListForRecord(), *m_writable_buffer,
                RHIResourceStateType::STATE_RENDER_TARGET, RHIResourceStateType::STATE_UNORDERED_ACCESS))

    RETURN_IF_FALSE(RHIUtils::Instance().AddRenderTargetBarrierToCommandList(resource_manager.GetCommandListForRecord(), *m_back_buffer,
                RHIResourceStateType::STATE_RENDER_TARGET, RHIResourceStateType::STATE_NON_PIXEL_SHADER_RESOURCE))
    
    return true;
}

bool RWTextureResourceWithBackBuffer::CreateDescriptors(glTFRenderResourceManager& resource_manager,
    IRHIDescriptorHeap& main_descriptor)
{
    RETURN_IF_FALSE(main_descriptor.CreateUnOrderAccessViewInDescriptorHeap(resource_manager.GetDevice(), main_descriptor.GetUsedDescriptorCount(),
                *m_writable_buffer, {m_writable_buffer->GetRenderTargetFormat(), RHIResourceDimension::TEXTURE2D}, m_writable_buffer_handle))

    RETURN_IF_FALSE(main_descriptor.CreateShaderResourceViewInDescriptorHeap(resource_manager.GetDevice(), main_descriptor.GetUsedDescriptorCount(),
                *m_back_buffer, {m_back_buffer->GetRenderTargetFormat(), RHIResourceDimension::TEXTURE2D}, m_back_buffer_handle))
    
    return true;
}

bool RWTextureResourceWithBackBuffer::RegisterSignature(IRHIRootSignatureHelper& root_signature)
{
    RETURN_IF_FALSE(root_signature.AddTableRootParameter(GetOutputBufferResourceName(), RHIRootParameterDescriptorRangeType::UAV, 1, false, m_writable_buffer_allocation))
    RETURN_IF_FALSE(root_signature.AddTableRootParameter(GetBackBufferResourceName(), RHIRootParameterDescriptorRangeType::SRV, 1, false, m_back_buffer_allocation))
    
    return true;
}

bool RWTextureResourceWithBackBuffer::CopyToBackBuffer(glTFRenderResourceManager& resource_manager)
{
    auto& command_list = resource_manager.GetCommandListForRecord();
    
    // Copy accumulation buffer to back buffer
    RETURN_IF_FALSE(RHIUtils::Instance().AddRenderTargetBarrierToCommandList(command_list, *m_writable_buffer,
        RHIResourceStateType::STATE_UNORDERED_ACCESS, RHIResourceStateType::STATE_COPY_SOURCE))

    RETURN_IF_FALSE(RHIUtils::Instance().AddRenderTargetBarrierToCommandList(command_list, *m_back_buffer,
        RHIResourceStateType::STATE_NON_PIXEL_SHADER_RESOURCE, RHIResourceStateType::STATE_COPY_DEST))

    RETURN_IF_FALSE(RHIUtils::Instance().CopyTexture(command_list, *m_back_buffer,  *m_writable_buffer))
    
    RETURN_IF_FALSE(RHIUtils::Instance().AddRenderTargetBarrierToCommandList(command_list, *m_writable_buffer,
        RHIResourceStateType::STATE_COPY_SOURCE, RHIResourceStateType::STATE_UNORDERED_ACCESS))
    
    RETURN_IF_FALSE(RHIUtils::Instance().AddRenderTargetBarrierToCommandList(command_list, *m_back_buffer,
        RHIResourceStateType::STATE_COPY_DEST, RHIResourceStateType::STATE_NON_PIXEL_SHADER_RESOURCE))
    
    return true;
}

std::string RWTextureResourceWithBackBuffer::GetOutputBufferResourceName() const
{
    return m_texture_desc.name;
}

std::string RWTextureResourceWithBackBuffer::GetBackBufferResourceName() const
{
    return GetOutputBufferResourceName() + "_BACK";
}

bool RWTextureResourceWithBackBuffer::AddShaderMacros(RHIShaderPreDefineMacros& macros)
{
    macros.AddUAVRegisterDefine(m_output_register_name, m_writable_buffer_allocation.register_index, m_writable_buffer_allocation.space);
    macros.AddSRVRegisterDefine(m_back_register_name, m_back_buffer_allocation.register_index, m_back_buffer_allocation.space);
    
    return true;
}

bool RWTextureResourceWithBackBuffer::BindRootParameter(glTFRenderResourceManager& resource_manager)
{
    auto& command_list = resource_manager.GetCommandListForRecord();
    
    RETURN_IF_FALSE(RHIUtils::Instance().SetDTToRootParameterSlot(command_list, m_writable_buffer_allocation.parameter_index, m_writable_buffer_handle,false))
    RETURN_IF_FALSE(RHIUtils::Instance().SetDTToRootParameterSlot(command_list, m_back_buffer_allocation.parameter_index, m_back_buffer_handle,false))
    
    return true;
}

glTFRayTracingPassPathTracing::glTFRayTracingPassPathTracing()
    : m_raytracing_output(nullptr)
    , m_trace_count({0, 0, 0})
    , m_accumulation_resource("ACCUMULATION_OUTPUT_REGISTER_INDEX", "ACCUMULATION_BACKBUFFER_REGISTER_INDEX")
    , m_custom_resource("CUSTOM_OUTPUT_REGISTER_INDEX", "CUSTOM_BACKBUFFER_REGISTER_INDEX")
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
    
    RETURN_IF_FALSE(glTFRayTracingPassBase::InitPass(resource_manager))
    
    RETURN_IF_FALSE(RHIUtils::Instance().AddRenderTargetBarrierToCommandList(resource_manager.GetCommandListForRecord(), *m_raytracing_output,
        RHIResourceStateType::STATE_RENDER_TARGET, RHIResourceStateType::STATE_UNORDERED_ACCESS))

    RETURN_IF_FALSE(UpdateAS(resource_manager))
    
    m_shader_table = RHIResourceFactory::CreateRHIResource<IRHIShaderTable>();

    const auto& meshes = resource_manager.GetMeshManager().GetMeshes();
    std::vector<std::shared_ptr<RHIShaderTableRecordBase>> hit_group_records(meshes.size());
    for (const auto& mesh_info : resource_manager.GetMeshManager().GetMeshes())
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
    RETURN_IF_FALSE(m_accumulation_resource.BindRootParameter(resource_manager))
    RETURN_IF_FALSE(m_custom_resource.BindRootParameter(resource_manager))
    
    RETURN_IF_FALSE(RHIUtils::Instance().SetSRVToRootParameterSlot(command_list, m_raytracing_as_allocation.parameter_index, m_raytracing_as->GetTLASHandle(), false)) 
    if (!m_material_uploaded)
    {
        GetRenderInterface<glTFRenderInterfaceSceneMaterial>()->UploadMaterialData(resource_manager, *m_main_descriptor_heap);
        m_material_uploaded = true;
    }

    RETURN_IF_FALSE(GetRenderInterface<glTFRenderInterfaceLighting>()->UpdateCPUBuffer())
    
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

    // Copy accumulation buffer to back buffer
    RETURN_IF_FALSE(m_accumulation_resource.CopyToBackBuffer(resource_manager))
    RETURN_IF_FALSE(m_custom_resource.CopyToBackBuffer(resource_manager))
    
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
    
    RETURN_IF_FALSE(m_accumulation_resource.RegisterSignature(m_root_signature_helper))
    RETURN_IF_FALSE(m_custom_resource.RegisterSignature(m_root_signature_helper))
    
    RETURN_IF_FALSE(m_root_signature_helper.AddSRVRootParameter("RaytracingAS", m_raytracing_as_allocation))
    
    RETURN_IF_FALSE(glTFRayTracingPassBase::SetupRootSignature(resource_manager))
    
    m_trace_count = {resource_manager.GetSwapchain().GetWidth(), resource_manager.GetSwapchain().GetHeight(), 1};

    return true;
}

bool glTFRayTracingPassPathTracing::SetupPipelineStateObject(glTFRenderResourceManager& resource_manager)
{
    RETURN_IF_FALSE(glTFRayTracingPassBase::SetupPipelineStateObject(resource_manager))

    RETURN_IF_FALSE(m_main_descriptor_heap->CreateUnOrderAccessViewInDescriptorHeap(resource_manager.GetDevice(), m_main_descriptor_heap->GetUsedDescriptorCount(),
                *m_raytracing_output, {m_raytracing_output->GetRenderTargetFormat(), RHIResourceDimension::TEXTURE2D}, m_output_handle))

    RETURN_IF_FALSE(m_accumulation_resource.CreateDescriptors(resource_manager, *m_main_descriptor_heap))
    RETURN_IF_FALSE(m_custom_resource.CreateDescriptors(resource_manager, *m_main_descriptor_heap))
    
    GetRayTracingPipelineStateObject().BindShaderCode("glTFResources/ShaderSource/RayTracing/PathTracingMain.hlsl",
        RHIShaderType::RayTracing, "");
    
    auto& shader_macros = GetRayTracingPipelineStateObject().GetShaderMacros();
    shader_macros.AddSRVRegisterDefine("SCENE_AS_REGISTER_INDEX", m_raytracing_as_allocation.register_index, m_raytracing_as_allocation.space);
    shader_macros.AddUAVRegisterDefine("OUTPUT_REGISTER_INDEX", m_output_allocation.register_index, m_output_allocation.space);
    
    RETURN_IF_FALSE(m_accumulation_resource.AddShaderMacros(shader_macros))
    RETURN_IF_FALSE(m_custom_resource.AddShaderMacros(shader_macros))
    
    const auto& bind_rt_shader = GetRayTracingPipelineStateObject().GetBindShader(RHIShaderType::RayTracing);
    
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
    RETURN_IF_FALSE(m_raytracing_as->InitRayTracingAS(resource_manager.GetDevice(), resource_manager.GetCommandListForRecord(), resource_manager.GetMeshManager().GetMeshes()))
    
    return true;
}
