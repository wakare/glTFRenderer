#ifndef SCENE_MESH_INFO
#define SCENE_MESH_INFO

struct SceneMeshStartIndexInfo
{
    uint start_index;
};
StructuredBuffer<SceneMeshStartIndexInfo> g_mesh_start_info : SceneMeshStartIndexInfo_REGISTER_SRV_INDEX;

struct SceneMeshIndexInfo
{
    uint vertex_index;
};
StructuredBuffer<SceneMeshIndexInfo> g_mesh_index_info : SceneMeshIndexInfo_REGISTER_SRV_INDEX;

struct SceneMeshVertexInfo
{
    float4 normal;
    float4 tangent;
    float4 uv;
};
StructuredBuffer<SceneMeshVertexInfo> g_mesh_vertex_info : SceneMeshVertexInfo_REGISTER_SRV_INDEX;

uint3 GetMeshTriangleVertexIndex(uint mesh_id, uint primitive_id)
{
    uint mesh_start_index = g_mesh_start_info[mesh_id].start_index;
    uint3 index = uint3(mesh_start_index + 3 * primitive_id, mesh_start_index + 3 * primitive_id + 1, mesh_start_index + 3 * primitive_id + 2);
    return uint3(g_mesh_index_info[index.x].vertex_index, g_mesh_index_info[index.y].vertex_index, g_mesh_index_info[index.z].vertex_index);
}

SceneMeshVertexInfo InterpolateVertexWithBarycentrics(uint3 vertex_index, float2 barycentrics)
{
    SceneMeshVertexInfo result;

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
