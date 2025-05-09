﻿#include "glTFRenderInterfaceSceneMeshInfo.h"

#include "glTFRenderInterfaceStructuredBuffer.h"

constexpr size_t max_heap_size = 256ull * 64 * 1024;

glTFRenderInterfaceSceneMeshInfo::glTFRenderInterfaceSceneMeshInfo()
{
    AddInterface(std::make_shared<glTFRenderInterfaceStructuredBuffer<SceneMeshDataOffsetInfo>>(SceneMeshDataOffsetInfo::Name.c_str()));
    AddInterface(std::make_shared<glTFRenderInterfaceStructuredBuffer<SceneMeshVertexInfo>>(SceneMeshVertexInfo::Name.c_str(), max_heap_size));
    AddInterface(std::make_shared<glTFRenderInterfaceStructuredBuffer<SceneMeshFaceInfo>>(SceneMeshFaceInfo::Name.c_str(), max_heap_size));
}

bool glTFRenderInterfaceSceneMeshInfo::UpdateSceneMeshData(glTFRenderResourceManager& resource_manager, const glTFRenderMeshManager& mesh_manager)
{
    const auto& meshes = mesh_manager.GetMeshRenderResources();

    std::vector<SceneMeshDataOffsetInfo> start_offset_infos;
    std::vector<SceneMeshVertexInfo> mesh_vertex_infos;
    std::vector<SceneMeshFaceInfo> mesh_face_vertex_infos;

    unsigned start_face_offset = 0;
    for (const auto& mesh : meshes)
    {
        const unsigned vertex_offset = static_cast<unsigned>(mesh_vertex_infos.size());
        
        SceneMeshDataOffsetInfo mesh_info =
        {
            .start_face_index = start_face_offset,
            .material_index = mesh.second.material_id,
            .start_vertex_index = vertex_offset,
        };
        
        start_offset_infos.push_back(mesh_info);
        const size_t face_count = mesh.second.mesh_index_count / 3;
        
        const auto& index_buffer = mesh.second.mesh->GetIndexBufferData();
        for (size_t i = 0; i < face_count; ++i)
        {
            mesh_face_vertex_infos.push_back(
            {
                vertex_offset + index_buffer.GetIndexByOffset(3 * i),
                vertex_offset + index_buffer.GetIndexByOffset(3 * i + 1),
                vertex_offset + index_buffer.GetIndexByOffset(3 * i + 2),
            });
        }

        for (size_t i = 0; i < mesh.second.mesh_vertex_count; ++i)
        {
            SceneMeshVertexInfo vertex_info;
            size_t out_data_size = 0;
            mesh.second.mesh->GetVertexBufferData().GetVertexAttributeDataByIndex(VertexAttributeType::VERTEX_POSITION, i, &vertex_info.position, out_data_size);
            mesh.second.mesh->GetVertexBufferData().GetVertexAttributeDataByIndex(VertexAttributeType::VERTEX_NORMAL, i, &vertex_info.normal, out_data_size);
            mesh.second.mesh->GetVertexBufferData().GetVertexAttributeDataByIndex(VertexAttributeType::VERTEX_TANGENT, i, &vertex_info.tangent, out_data_size);
            mesh.second.mesh->GetVertexBufferData().GetVertexAttributeDataByIndex(VertexAttributeType::VERTEX_TEXCOORD0, i, &vertex_info.uv, out_data_size);
            mesh_vertex_infos.push_back(vertex_info);
        }
        
        start_face_offset += face_count;
    }

    GetRenderInterface<glTFRenderInterfaceStructuredBuffer<SceneMeshDataOffsetInfo>>()->UploadBuffer(resource_manager,start_offset_infos.data(), 0, start_offset_infos.size() * sizeof(SceneMeshDataOffsetInfo));
    GetRenderInterface<glTFRenderInterfaceStructuredBuffer<SceneMeshVertexInfo>>()->UploadBuffer(resource_manager,mesh_vertex_infos.data(), 0, mesh_vertex_infos.size() * sizeof(SceneMeshVertexInfo));
    GetRenderInterface<glTFRenderInterfaceStructuredBuffer<SceneMeshFaceInfo>>()->UploadBuffer(resource_manager,mesh_face_vertex_infos.data(), 0, mesh_face_vertex_infos.size() * sizeof(SceneMeshFaceInfo));

    return true;
}
