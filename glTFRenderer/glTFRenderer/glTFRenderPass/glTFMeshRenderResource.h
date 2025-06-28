#pragma once
#include <memory>

#include "RHIInterface/RHIIndexBuffer.h"
#include "RHIInterface/RHIVertexBuffer.h"

class glTFScenePrimitive;

struct glTFMeshRenderResource
{
    const glTFScenePrimitive* mesh;
    
    // Vertex and index gpu buffer data
    std::shared_ptr<RHIVertexBuffer> mesh_vertex_buffer;
    std::shared_ptr<RHIVertexBuffer> mesh_position_only_buffer;
    std::shared_ptr<RHIIndexBuffer> mesh_index_buffer;
    std::shared_ptr<IRHIVertexBufferView> mesh_vertex_buffer_view;
    std::shared_ptr<IRHIVertexBufferView> mesh_position_only_buffer_view;
    std::shared_ptr<IRHIIndexBufferView> mesh_index_buffer_view;

    size_t mesh_vertex_count{0};
    size_t mesh_index_count{0};
    
    glTFUniqueID material_id {glTFUniqueIDInvalid};
    bool using_normal_mapping {false};
};

