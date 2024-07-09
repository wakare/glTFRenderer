#include "DX12VertexBuffer.h"

#include "glTFRenderPass/glTFRenderResourceManager.h"
#include "glTFRHI/RHIResourceFactoryImpl.hpp"

std::shared_ptr<IRHIVertexBufferView> DX12VertexBuffer::CreateVertexBufferView(IRHIDevice& device,
    IRHICommandList& command_list, const RHIBufferDesc& desc, const VertexBufferData& vertex_buffer_data)
{
    m_vertex_layout = vertex_buffer_data.layout;
    m_vertex_count = vertex_buffer_data.vertex_count;
    
    glTFRenderResourceManager::GetMemoryManager().AllocateBufferMemory(device, desc, m_buffer);

    const RHIBufferDesc vertex_upload_buffer_desc = {L"vertexBufferUploadBuffer", vertex_buffer_data.byteSize,
        1, 1, RHIBufferType::Upload, RHIDataFormat::UNKNOWN, RHIBufferResourceType::Buffer};
    glTFRenderResourceManager::GetMemoryManager().AllocateBufferMemory(device, vertex_upload_buffer_desc, m_upload_buffer);
    
    RHIUtils::Instance().UploadBufferDataToDefaultGPUBuffer(command_list, *m_upload_buffer->m_buffer, *m_buffer->m_buffer, vertex_buffer_data.data.get(), vertex_buffer_data.byteSize);
    RHIUtils::Instance().AddBufferBarrierToCommandList(command_list, *m_buffer->m_buffer, RHIResourceStateType::STATE_COPY_DEST, RHIResourceStateType::STATE_VERTEX_AND_CONSTANT_BUFFER);

    auto vertex_buffer_view = RHIResourceFactory::CreateRHIResource<IRHIVertexBufferView>();
    vertex_buffer_view->InitVertexBufferView(*m_buffer->m_buffer, 0, vertex_buffer_data.layout.GetVertexStrideInBytes(), vertex_buffer_data.byteSize);

    return vertex_buffer_view;
}
