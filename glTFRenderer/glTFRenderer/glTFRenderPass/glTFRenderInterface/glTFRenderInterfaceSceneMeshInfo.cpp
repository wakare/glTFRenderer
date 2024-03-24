#include "glTFRenderInterfaceSceneMeshInfo.h"

#include "glTFRenderInterfaceStructuredBuffer.h"

constexpr size_t max_heap_size = 256ull * 64 * 1024;

glTFRenderInterfaceSceneMeshInfo::glTFRenderInterfaceSceneMeshInfo()
{
    AddInterface(std::make_shared<glTFRenderInterfaceStructuredBuffer<SceneMeshDataOffsetInfo>>());
    AddInterface(std::make_shared<glTFRenderInterfaceStructuredBuffer<SceneMeshVertexInfo, max_heap_size>>());
    AddInterface(std::make_shared<glTFRenderInterfaceStructuredBuffer<SceneMeshFaceInfo, max_heap_size>>());
}

bool glTFRenderInterfaceSceneMeshInfo::UpdateSceneMeshData(const glTFRenderMeshManager& mesh_manager)
{
    const auto& meshes = mesh_manager.GetMeshRenderResources();

    std::vector<SceneMeshDataOffsetInfo> start_offset_infos;
    std::vector<SceneMeshVertexInfo> vertex_infos;
    std::vector<SceneMeshFaceInfo> index_infos;

    unsigned start_face_offset = 0;
    for (const auto& mesh : meshes)
    {
        start_offset_infos.push_back({start_face_offset, mesh.second.material_id});
        const size_t face_count = mesh.second.mesh_index_count / 3;
        
        const unsigned vertex_offset = static_cast<unsigned>(vertex_infos.size());
        const auto& index_buffer = mesh.second.mesh->GetIndexBufferData();
        for (size_t i = 0; i < face_count; ++i)
        {
            index_infos.push_back(
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
            mesh.second.mesh->GetVertexBufferData().GetVertexAttributeDataByIndex(VertexAttributeType::POSITION, i, &vertex_info.position, out_data_size);
            mesh.second.mesh->GetVertexBufferData().GetVertexAttributeDataByIndex(VertexAttributeType::NORMAL, i, &vertex_info.normal, out_data_size);
            mesh.second.mesh->GetVertexBufferData().GetVertexAttributeDataByIndex(VertexAttributeType::TANGENT, i, &vertex_info.tangent, out_data_size);
            mesh.second.mesh->GetVertexBufferData().GetVertexAttributeDataByIndex(VertexAttributeType::TEXCOORD_0, i, &vertex_info.uv, out_data_size);
            vertex_infos.push_back(vertex_info);
        }
        
        start_face_offset += face_count;
    }

    GetRenderInterface<glTFRenderInterfaceStructuredBuffer<SceneMeshDataOffsetInfo>>()->UploadCPUBuffer(start_offset_infos.data(),0, start_offset_infos.size() * sizeof(SceneMeshDataOffsetInfo));
    GetRenderInterface<glTFRenderInterfaceStructuredBuffer<SceneMeshVertexInfo, max_heap_size>>()->UploadCPUBuffer(vertex_infos.data(),0, vertex_infos.size() * sizeof(SceneMeshVertexInfo));
    GetRenderInterface<glTFRenderInterfaceStructuredBuffer<SceneMeshFaceInfo, max_heap_size>>()->UploadCPUBuffer(index_infos.data(),0, index_infos.size() * sizeof(SceneMeshFaceInfo));

    return true;
}
