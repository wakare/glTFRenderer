#include "glTFRayTracingPassWithMesh.h"

#include "glTFRenderPass/glTFRenderInterface/glTFRenderInterfaceFrameStat.h"
#include "glTFRenderPass/glTFRenderInterface/glTFRenderInterfaceLighting.h"
#include "glTFRenderPass/glTFRenderInterface/glTFRenderInterfaceSceneMaterial.h"
#include "glTFRenderPass/glTFRenderInterface/glTFRenderInterfaceSceneMeshInfo.h"
#include "glTFRenderPass/glTFRenderInterface/glTFRenderInterfaceViewBase.h"
#include "RHIResourceFactoryImpl.hpp"

#include <cstring>
#include <unordered_map>

glTFRayTracingPassWithMesh::glTFRayTracingPassWithMesh()
{
    
}

bool glTFRayTracingPassWithMesh::InitRenderInterface(glTFRenderResourceManager& resource_manager)
{
    RETURN_IF_FALSE(glTFRayTracingPassBase::InitRenderInterface(resource_manager))
    
    AddRenderInterface(std::make_shared<glTFRenderInterfaceFrameStat>());
    AddRenderInterface(std::make_shared<glTFRenderInterfaceSceneView>());
    AddRenderInterface(std::make_shared<glTFRenderInterfaceSceneMeshInfo>());
    AddRenderInterface(std::make_shared<glTFRenderInterfaceLighting>());
    AddRenderInterface(std::make_shared<glTFRenderInterfaceSceneMaterial>());
    
    return true;
}

bool glTFRayTracingPassWithMesh::InitPass(glTFRenderResourceManager& resource_manager)
{
    RETURN_IF_FALSE(glTFRayTracingPassBase::InitPass(resource_manager))
    
    RETURN_IF_FALSE(GetRenderInterface<glTFRenderInterfaceSceneMeshInfo>()->UpdateSceneMeshData(resource_manager, resource_manager.GetMeshManager()))
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

    m_ray_type_count = static_cast<uint32_t>(sbts.size());
    RETURN_IF_FALSE(UpdateAS(resource_manager))
    
    RETURN_IF_FALSE(m_shader_table->InitShaderTable(resource_manager.GetDevice(), resource_manager.GetCommandListForRecord(), resource_manager.GetMemoryManager(), GetRayTracingPipelineStateObject(), *m_raytracing_as, sbts))

    m_trace_count = {resource_manager.GetSwapChain().GetWidth(), resource_manager.GetSwapChain().GetHeight(), 1};
    
    return true;
}

bool glTFRayTracingPassWithMesh::PreRenderPass(glTFRenderResourceManager& resource_manager)
{
    RETURN_IF_FALSE(glTFRayTracingPassBase::PreRenderPass(resource_manager))
    RETURN_IF_FALSE(UpdateAS(resource_manager))
    BindDescriptor(resource_manager.GetCommandListForRecord(), m_raytracing_as_allocation, m_raytracing_as->GetTLASDescriptorSRV());

    return true;
}

bool glTFRayTracingPassWithMesh::PostRenderPass(glTFRenderResourceManager& resource_manager)
{
    RETURN_IF_FALSE(glTFRayTracingPassBase::PostRenderPass(resource_manager))
    return true;
}

bool glTFRayTracingPassWithMesh::TryProcessSceneObject(glTFRenderResourceManager& resource_manager,
    const glTFSceneObjectBase& object)
{
    RETURN_IF_FALSE(glTFRayTracingPassBase::TryProcessSceneObject(resource_manager, object))
    
    const glTFLightBase* light = dynamic_cast<const glTFLightBase*>(&object);
    if (!light)
    {
        return false;
    }
    
    return GetRenderInterface<glTFRenderInterfaceLighting>()->UpdateLightInfo(*light);
}

TraceCount glTFRayTracingPassWithMesh::GetTraceCount() const
{
    return m_trace_count;
}

IRHIShaderTable& glTFRayTracingPassWithMesh::GetShaderTable() const
{
    return *m_shader_table;
}

bool glTFRayTracingPassWithMesh::SetupRootSignature(glTFRenderResourceManager& resource_manager)
{
    RETURN_IF_FALSE(m_root_signature_helper.AddSRVRootParameter("SCENE_AS_REGISTER_INDEX", m_raytracing_as_allocation))
    RETURN_IF_FALSE(glTFRayTracingPassBase::SetupRootSignature(resource_manager))
    
    return true;
}

bool glTFRayTracingPassWithMesh::SetupPipelineStateObject(glTFRenderResourceManager& resource_manager)
{
    RETURN_IF_FALSE(glTFRayTracingPassBase::SetupPipelineStateObject(resource_manager))
    
    auto& shader_macros = GetShaderMacros();
    m_raytracing_as_allocation.AddShaderDefine(shader_macros);
    return true;
}

bool glTFRayTracingPassWithMesh::UpdateAS(glTFRenderResourceManager& resource_manager)
{
    if (!m_raytracing_as)
    {
        RETURN_IF_FALSE(BuildAS(resource_manager))
    }

    return true;
}

bool glTFRayTracingPassWithMesh::BuildAS(glTFRenderResourceManager& resource_manager)
{
    m_raytracing_as = RHIResourceFactory::CreateRHIResource<IRHIRayTracingAS>();
    
    RHIRayTracingSceneDesc scene_desc;
    const auto& mesh_render_resources = resource_manager.GetMeshManager().GetMeshRenderResources();
    const auto& mesh_instances = resource_manager.GetMeshManager().GetMeshInstanceRenderResource();

    scene_desc.geometries.reserve(mesh_render_resources.size());
    std::unordered_map<RendererUniqueObjectID, uint32_t> geometry_index_map;
    for (const auto& mesh_pair : mesh_render_resources)
    {
        const auto& mesh = mesh_pair.second;
        RHIRayTracingGeometryDesc geometry_desc{};
        geometry_desc.vertex_buffer = mesh.mesh_position_only_buffer ? mesh.mesh_position_only_buffer : mesh.mesh_vertex_buffer;
        geometry_desc.index_buffer = mesh.mesh_index_buffer;
        geometry_desc.vertex_count = mesh.mesh_vertex_count;
        geometry_desc.index_count = mesh.mesh_index_count;
        geometry_desc.opaque = true;

        geometry_index_map[mesh_pair.first] = static_cast<uint32_t>(scene_desc.geometries.size());
        scene_desc.geometries.push_back(geometry_desc);
    }

    scene_desc.instances.reserve(mesh_instances.size());
    uint32_t instance_index = 0;
    for (const auto& instance_pair : mesh_instances)
    {
        const auto& instance = instance_pair.second;
        auto geom_it = geometry_index_map.find(instance.m_mesh_render_resource);
        if (geom_it == geometry_index_map.end())
        {
            continue;
        }

        RHIRayTracingInstanceDesc instance_desc{};
        memcpy(instance_desc.transform.data(), &instance.m_instance_transform[0][0], sizeof(instance_desc.transform));
        instance_desc.instance_id = instance_index;
        instance_desc.instance_mask = 0xFF;
        instance_desc.geometry_index = geom_it->second;
        instance_desc.hit_group_index = instance_index * std::max(1u, m_ray_type_count);

        scene_desc.instances.push_back(instance_desc);
        ++instance_index;
    }

    m_raytracing_as->SetRayTracingSceneDesc(scene_desc);
    RETURN_IF_FALSE(m_raytracing_as->InitRayTracingAS(resource_manager.GetDevice(), resource_manager.GetCommandListForRecord(), resource_manager.GetMemoryManager()))
    
    return true;
}
