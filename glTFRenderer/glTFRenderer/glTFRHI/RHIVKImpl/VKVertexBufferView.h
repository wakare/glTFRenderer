#pragma once
#include "VKBuffer.h"
#include "glTFRHI/RHIInterface/IRHIVertexBufferView.h"

class VKVertexBufferView : public IRHIVertexBufferView
{
public:
    virtual void InitVertexBufferView(IRHIBuffer& buffer, size_t offset, size_t vertexStride, size_t vertex_buffer_size) override;

    VkBuffer m_buffer;
    size_t m_buffer_offset;
    size_t m_buffer_size;
};
