#include "IRHIIndexBuffer.h"
#include "glTFRHI/RHIResourceFactoryImpl.hpp"

IRHIIndexBuffer::IRHIIndexBuffer()
    : m_buffer(nullptr)
    , m_upload_buffer(nullptr)
    , m_index_format(RHIDataFormat::Unknown)
    , m_index_count(0)
{
    m_buffer = RHIResourceFactory::CreateRHIResource<IRHIGPUBuffer>();
    m_upload_buffer = RHIResourceFactory::CreateRHIResource<IRHIGPUBuffer>();
}
