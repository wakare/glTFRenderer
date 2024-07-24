#include "IRHIIndexBuffer.h"
#include "glTFRHI/RHIResourceFactoryImpl.hpp"

std::shared_ptr<IRHIIndexBufferView> IRHIIndexBuffer::CreateIndexBufferView(IRHIDevice& device,
    IRHIMemoryManager& memory_manager, IRHICommandList& command_list, const RHIBufferDesc& desc,
    const IndexBufferData& index_buffer_data)
{
    m_index_format = index_buffer_data.format;
    m_index_count = index_buffer_data.index_count;

    memory_manager.AllocateBufferMemory( device, desc, m_buffer);

    memory_manager.AllocateBufferMemory(
    device,
    {
        L"indexBufferUploadBuffer",
        index_buffer_data.byte_size,
        1,
        1,
        RHIBufferType::Upload,
        RHIDataFormat::UNKNOWN,
        RHIBufferResourceType::Buffer,
        RHIResourceStateType::STATE_COMMON,
        static_cast<RHIResourceUsageFlags>(RUF_TRANSFER_SRC | RUF_TRANSFER_DST)
    },
    m_upload_buffer);

    memory_manager.UploadBufferData(*m_upload_buffer, index_buffer_data.data.get(), 0, index_buffer_data.byte_size);
    RHIUtils::Instance().CopyBuffer(command_list, *m_buffer->m_buffer, 0, *m_upload_buffer->m_buffer, 0, index_buffer_data.byte_size);
    
    auto index_buffer_view = RHIResourceFactory::CreateRHIResource<IRHIIndexBufferView>();
    RHIIndexBufferViewDesc index_buffer_desc{};
    index_buffer_desc.size = index_buffer_data.byte_size;
    index_buffer_desc.offset = 0;
    index_buffer_desc.format = index_buffer_data.format;
    index_buffer_view->InitIndexBufferView(*m_buffer->m_buffer, index_buffer_desc);
    
    return index_buffer_view;
}
