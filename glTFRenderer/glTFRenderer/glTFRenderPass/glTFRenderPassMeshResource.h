#pragma once
#include <memory>

#include "glTFRHI/RHIInterface/IRHIIndexBuffer.h"
#include "glTFRHI/RHIInterface/IRHIVertexBuffer.h"

struct glTFRenderPassMeshResource
{
    // Vertex and index gpu buffer data
    std::shared_ptr<IRHIVertexBuffer> mesh_vertex_buffer;
    std::shared_ptr<IRHIVertexBuffer> mesh_position_only_buffer;
    std::shared_ptr<IRHIIndexBuffer> mesh_index_buffer;
    std::shared_ptr<IRHIVertexBufferView> mesh_vertex_buffer_view;
    std::shared_ptr<IRHIVertexBufferView> mesh_position_only_buffer_view;
    std::shared_ptr<IRHIIndexBufferView> mesh_index_buffer_view;

    size_t mesh_vertex_count{0};
    size_t mesh_index_count{0};
    
    glm::mat4 mesh_transform_matrix{1.0f};
    glTFUniqueID material_id {glTFUniqueIDInvalid};
    bool using_normal_mapping {false};
};

