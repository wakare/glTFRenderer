#include "DX12IndexBuffer.h"

#include "glTFRenderPass/glTFRenderResourceManager.h"
#include "glTFRHI/RHIResourceFactoryImpl.hpp"

std::shared_ptr<IRHIIndexBufferView> DX12IndexBuffer::CreateIndexBufferView(IRHIDevice& device,
                                                                            IRHICommandList& command_list, glTFRenderResourceManager& resource_manager, const RHIBufferDesc& desc, const IndexBufferData& index_buffer_data)
{
    m_index_format = index_buffer_data.format;
    m_index_count = index_buffer_data.index_count;

    resource_manager.GetMemoryManager().AllocateBufferMemory(
    device,
    {
        L"indexBufferBuffer",
        index_buffer_data.byteSize,
        1,
        1,
        RHIBufferType::Default,
        RHIDataFormat::UNKNOWN,
        RHIBufferResourceType::Buffer
    },
    m_buffer);

    resource_manager.GetMemoryManager().AllocateBufferMemory(
    device,
    {
        L"indexBufferUploadBuffer",
        index_buffer_data.byteSize,
        1,
        1,
        RHIBufferType::Upload,
        RHIDataFormat::UNKNOWN,
        RHIBufferResourceType::Buffer
    },
    m_upload_buffer);
    
    RHIUtils::Instance().UploadBufferDataToDefaultGPUBuffer(command_list, *m_upload_buffer->m_buffer, *m_buffer->m_buffer, index_buffer_data.data.get(), index_buffer_data.byteSize);
    RHIUtils::Instance().AddBufferBarrierToCommandList(command_list, *m_buffer->m_buffer, RHIResourceStateType::STATE_COPY_DEST, RHIResourceStateType::STATE_INDEX_BUFFER);

    auto index_buffer_view = RHIResourceFactory::CreateRHIResource<IRHIIndexBufferView>();
    RHIIndexBufferViewDesc index_buffer_desc{};
    index_buffer_desc.size = index_buffer_data.byteSize;
    index_buffer_desc.offset = 0;
    index_buffer_desc.format = index_buffer_data.format;
    index_buffer_view->InitIndexBufferView(*m_buffer->m_buffer, index_buffer_desc);

    return index_buffer_view;
}
