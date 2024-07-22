#ifndef SCENE_MESH_INFO
#define SCENE_MESH_INFO

struct SceneMeshDataOffsetInfo
{
    uint start_face_index;
    uint material_index;
    uint start_vertex_index;
};
StructuredBuffer<SceneMeshDataOffsetInfo> g_mesh_start_info : SceneMeshDataOffsetInfo_REGISTER_SRV_INDEX;

struct SceneMeshFaceInfo
{
    uint3 vertex_index;
};
StructuredBuffer<SceneMeshFaceInfo> g_mesh_face_info : SceneMeshFaceInfo_REGISTER_SRV_INDEX;

struct SceneMeshVertexInfo
{
    float4 position;
    float4 normal;
    float4 tangent;
    float4 uv;
};
StructuredBuffer<SceneMeshVertexInfo> g_mesh_vertex_info : SceneMeshVertexInfo_REGISTER_SRV_INDEX;

uint3 GetMeshTriangleVertexIndex(uint mesh_id, uint primitive_id)
{
    uint primitive_face_offset = g_mesh_start_info[mesh_id].start_face_index + primitive_id;
    return g_mesh_face_info[primitive_face_offset].vertex_index;
}

uint GetMeshMaterialId(uint mesh_id)
{
    return g_mesh_start_info[mesh_id].material_index;
}

SceneMeshVertexInfo InterpolateVertexWithBarycentrics(uint3 vertex_index, float2 barycentrics)
{
    SceneMeshVertexInfo result;

    result.position = (g_mesh_vertex_info[vertex_index.x].position * (1 - barycentrics.x - barycentrics.y) +
            g_mesh_vertex_info[vertex_index.y].position * barycentrics.x +
            g_mesh_vertex_info[vertex_index.z].position * barycentrics.y);
    
    result.normal = (g_mesh_vertex_info[vertex_index.x].normal * (1 - barycentrics.x - barycentrics.y) +
            g_mesh_vertex_info[vertex_index.y].normal * barycentrics.x +
            g_mesh_vertex_info[vertex_index.z].normal * barycentrics.y);
    
    result.tangent = (g_mesh_vertex_info[vertex_index.x].tangent * (1 - barycentrics.x - barycentrics.y) +
            g_mesh_vertex_info[vertex_index.y].tangent * barycentrics.x +
            g_mesh_vertex_info[vertex_index.z].tangent * barycentrics.y);
    
    result.uv = (g_mesh_vertex_info[vertex_index.x].uv * (1 - barycentrics.x - barycentrics.y) +
        g_mesh_vertex_info[vertex_index.y].uv * barycentrics.x +
        g_mesh_vertex_info[vertex_index.z].uv * barycentrics.y);
    
    return result;
}

#endif
