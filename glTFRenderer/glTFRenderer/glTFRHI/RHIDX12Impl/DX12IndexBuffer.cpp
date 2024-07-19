#include "DX12IndexBuffer.h"

#include "glTFRenderPass/glTFRenderResourceManager.h"
#include "glTFRHI/RHIResourceFactoryImpl.hpp"

std::shared_ptr<IRHIIndexBufferView> DX12IndexBuffer::CreateIndexBufferView(IRHIDevice& device, IRHIMemoryManager& memory_manager,
                                                                            IRHICommandList& command_list, const RHIBufferDesc& desc, const IndexBufferData& index_buffer_data)
{
    m_index_format = index_buffer_data.format;
    m_index_count = index_buffer_data.index_count;

    memory_manager.AllocateBufferMemory(
    device,
    {
        L"indexBufferBuffer",
        index_buffer_data.byte_size,
        1,
        1,
        RHIBufferType::Default,
        RHIDataFormat::UNKNOWN,
        RHIBufferResourceType::Buffer
    },
    m_buffer);

    memory_manager.AllocateBufferMemory(
    device,
    {
        L"indexBufferUploadBuffer",
        index_buffer_data.byte_size,
        1,
        1,
        RHIBufferType::Upload,
        RHIDataFormat::UNKNOWN,
        RHIBufferResourceType::Buffer
    },
    m_upload_buffer);
    
    RHIUtils::Instance().UploadBufferDataToDefaultGPUBuffer(command_list, *m_upload_buffer->m_buffer, *m_buffer->m_buffer, index_buffer_data.data.get(), index_buffer_data.byte_size);
    RHIUtils::Instance().AddBufferBarrierToCommandList(command_list, *m_buffer->m_buffer, RHIResourceStateType::STATE_COPY_DEST, RHIResourceStateType::STATE_INDEX_BUFFER);

    auto index_buffer_view = RHIResourceFactory::CreateRHIResource<IRHIIndexBufferView>();
    RHIIndexBufferViewDesc index_buffer_desc{};
    index_buffer_desc.size = index_buffer_data.byte_size;
    index_buffer_desc.offset = 0;
    index_buffer_desc.format = index_buffer_data.format;
    index_buffer_view->InitIndexBufferView(*m_buffer->m_buffer, index_buffer_desc);

    return index_buffer_view;
}
