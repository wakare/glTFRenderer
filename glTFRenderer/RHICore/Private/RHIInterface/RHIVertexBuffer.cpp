#include "RHIVertexBuffer.h"

#include "RHIResourceFactoryImpl.hpp"
#include "RHIUtils.h"

std::shared_ptr<IRHIVertexBufferView> RHIVertexBuffer::CreateVertexBufferView(IRHIDevice& device,
                                                                               IRHIMemoryManager& memory_manager, IRHICommandList& command_list, const RHIBufferDesc& desc,
                                                                               const VertexBufferData& vertex_buffer_data)
{
    m_vertex_layout = vertex_buffer_data.layout;
    m_vertex_count = vertex_buffer_data.vertex_count;
    
    memory_manager.AllocateBufferMemory(device, desc, m_buffer);

    const RHIBufferDesc vertex_upload_buffer_desc =
        {
        L"vertexBufferUploadBuffer",
        vertex_buffer_data.byte_size,
        1,
        1,
        RHIBufferType::Upload,
        RHIBufferResourceType::Buffer,
        RHIResourceStateType::STATE_COMMON,
        static_cast<RHIResourceUsageFlags>(RUF_TRANSFER_SRC | RUF_TRANSFER_DST)  
    };
    
    memory_manager.AllocateBufferMemory(device, vertex_upload_buffer_desc, m_upload_buffer);
    memory_manager.UploadBufferData(device, command_list, *m_upload_buffer, vertex_buffer_data.data.get(), 0, vertex_buffer_data.byte_size);
    
    m_buffer->m_buffer->Transition(command_list, RHIResourceStateType::STATE_COPY_DEST);
    RHIUtilInstanceManager::Instance().CopyBuffer(command_list, *m_buffer->m_buffer, 0, *m_upload_buffer->m_buffer, 0, vertex_buffer_data.byte_size);
    m_buffer->m_buffer->Transition(command_list, RHIResourceStateType::STATE_VERTEX_AND_CONSTANT_BUFFER);

    auto vertex_buffer_view = RHIResourceFactory::CreateRHIResource<IRHIVertexBufferView>();
    vertex_buffer_view->InitVertexBufferView(*m_buffer->m_buffer, 0, vertex_buffer_data.layout.GetVertexStrideInBytes(), vertex_buffer_data.byte_size);

    return vertex_buffer_view;
}
