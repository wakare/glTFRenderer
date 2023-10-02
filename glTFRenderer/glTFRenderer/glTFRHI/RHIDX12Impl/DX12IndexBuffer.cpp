#include "DX12IndexBuffer.h"
#include "glTFRHI/RHIResourceFactoryImpl.hpp"

std::shared_ptr<IRHIIndexBufferView> DX12IndexBuffer::CreateIndexBufferView(IRHIDevice& device,
    IRHICommandList& command_list, const RHIBufferDesc& desc, const IndexBufferData& index_buffer_data)
{
    m_buffer->InitGPUBuffer(device, desc);

    const RHIBufferDesc index_upload_buffer_desc = {L"indexBufferUploadBuffer", index_buffer_data.byteSize,
        1, 1, RHIBufferType::Upload, RHIDataFormat::Unknown, RHIBufferResourceType::Buffer};
    
    m_upload_buffer->InitGPUBuffer(device, index_upload_buffer_desc );
    RHIUtils::Instance().UploadBufferDataToDefaultGPUBuffer(command_list, *m_upload_buffer, *m_buffer, index_buffer_data.data.get(), index_buffer_data.byteSize);
    RHIUtils::Instance().AddBufferBarrierToCommandList(command_list, *m_buffer, RHIResourceStateType::COPY_DEST, RHIResourceStateType::INDEX_BUFFER);

    auto vertex_buffer_view = RHIResourceFactory::CreateRHIResource<IRHIIndexBufferView>();
    vertex_buffer_view->InitIndexBufferView(*m_buffer, 0, index_buffer_data.format, index_buffer_data.byteSize);

    return vertex_buffer_view;
}
