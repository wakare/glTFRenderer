﻿#include "glTFRenderInterfaceSceneMeshInfo.h"

#include "glTFRenderInterfaceStructuredBuffer.h"

constexpr size_t max_heap_size = 64ull * 64 * 1024;

glTFRenderInterfaceSceneMeshInfo::glTFRenderInterfaceSceneMeshInfo()
{
    AddInterface(std::make_shared<glTFRenderInterfaceStructuredBuffer<SceneMeshStartIndexInfo>>());
    AddInterface(std::make_shared<glTFRenderInterfaceStructuredBuffer<SceneMeshVertexInfo, max_heap_size>>());
    AddInterface(std::make_shared<glTFRenderInterfaceStructuredBuffer<SceneMeshIndexInfo, max_heap_size>>());
}

bool glTFRenderInterfaceSceneMeshInfo::UpdateSceneMeshData(const glTFRenderMeshManager& mesh_manager)
{
    const auto& meshes = mesh_manager.GetMeshes();

    std::vector<SceneMeshStartIndexInfo> start_index_infos;
    std::vector<SceneMeshVertexInfo> vertex_infos;
    std::vector<SceneMeshIndexInfo> index_infos;

    unsigned start_index = 0;
    for (const auto& mesh : meshes)
    {
        start_index_infos.push_back({start_index});

        const unsigned vertex_offset = vertex_infos.size();
        if (mesh.second.mesh->GetIndexBufferData().format == RHIDataFormat::R32_UINT)
        {
            const auto* index_data = reinterpret_cast<const unsigned*>(mesh.second.mesh->GetIndexBufferData().data.get());
            for (size_t i = 0; i < mesh.second.mesh_index_count; ++i)
            {
                const unsigned vertex_index = vertex_offset + index_data[i];
                index_infos.push_back({vertex_index});
            }    
        }
        else if (mesh.second.mesh->GetIndexBufferData().format == RHIDataFormat::R16_UINT)
        {
            const auto* index_data = reinterpret_cast<const uint16_t*>(mesh.second.mesh->GetIndexBufferData().data.get());
            for (size_t i = 0; i < mesh.second.mesh_index_count; ++i)
            {
                const unsigned vertex_index = vertex_offset + index_data[i];
                index_infos.push_back({vertex_index});
            }
        }

        for (size_t i = 0; i < mesh.second.mesh_vertex_count; ++i)
        {
            SceneMeshVertexInfo vertex_info;
            size_t out_data_size = 0;
            mesh.second.mesh->GetVertexBufferData().GetVertexAttributeDataByIndex(VertexLayoutType::TEXCOORD_0, i, &vertex_info, out_data_size);
            vertex_infos.push_back(vertex_info);
        }
        
        start_index += mesh.second.mesh_index_count;
    }

    GetRenderInterface<glTFRenderInterfaceStructuredBuffer<SceneMeshStartIndexInfo>>()->UploadCPUBuffer(start_index_infos.data(),start_index_infos.size() * sizeof(SceneMeshStartIndexInfo));
    GetRenderInterface<glTFRenderInterfaceStructuredBuffer<SceneMeshVertexInfo, max_heap_size>>()->UploadCPUBuffer(vertex_infos.data(),vertex_infos.size() * sizeof(SceneMeshVertexInfo));
    GetRenderInterface<glTFRenderInterfaceStructuredBuffer<SceneMeshIndexInfo, max_heap_size>>()->UploadCPUBuffer(index_infos.data(),index_infos.size() * sizeof(SceneMeshIndexInfo));
    
    return true;
}