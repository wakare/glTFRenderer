#include "VKVertexBufferView.h"

#include "VKBuffer.h"

void VKVertexBufferView::InitVertexBufferView(IRHIBuffer& buffer, size_t offset, size_t vertexStride,
                                              size_t vertex_buffer_size)
{
    m_buffer = dynamic_cast<VKBuffer&>(buffer).GetRawBuffer();
    m_buffer_offset = offset;
    m_buffer_size = vertex_buffer_size;
}
