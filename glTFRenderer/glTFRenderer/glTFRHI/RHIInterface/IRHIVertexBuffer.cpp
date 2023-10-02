#include "IRHIVertexBuffer.h"

#include "glTFRHI/RHIResourceFactoryImpl.hpp"

IRHIVertexBuffer::IRHIVertexBuffer()
    : m_buffer(nullptr)
    , m_upload_buffer(nullptr)
    , m_vertex_count(0)
{
    m_buffer = RHIResourceFactory::CreateRHIResource<IRHIGPUBuffer>();
    m_upload_buffer = RHIResourceFactory::CreateRHIResource<IRHIGPUBuffer>();
}
