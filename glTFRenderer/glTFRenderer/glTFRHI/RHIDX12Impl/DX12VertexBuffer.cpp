#include "DX12VertexBuffer.h"
#include "glTFRHI/RHIResourceFactoryImpl.hpp"

std::shared_ptr<IRHIVertexBufferView> DX12VertexBuffer::CreateVertexBufferView(IRHIDevice& device,
    IRHICommandList& command_list, const RHIBufferDesc& desc, const VertexBufferData& vertex_buffer_data)
{
    m_buffer->InitGPUBuffer(device, desc);

    const RHIBufferDesc vertex_upload_buffer_desc = {L"vertexBufferUploadBuffer", vertex_buffer_data.byteSize,
        1, 1, RHIBufferType::Upload, RHIDataFormat::Unknown, RHIBufferResourceType::Buffer};
    m_upload_buffer->InitGPUBuffer(device, vertex_upload_buffer_desc );
    RHIUtils::Instance().UploadBufferDataToDefaultGPUBuffer(command_list, *m_upload_buffer, *m_buffer, vertex_buffer_data.data.get(), vertex_buffer_data.byteSize);
    RHIUtils::Instance().AddBufferBarrierToCommandList(command_list, *m_buffer, RHIResourceStateType::COPY_DEST, RHIResourceStateType::VERTEX_AND_CONSTANT_BUFFER);

    auto vertex_buffer_view = RHIResourceFactory::CreateRHIResource<IRHIVertexBufferView>();
    vertex_buffer_view->InitVertexBufferView(*m_buffer, 0, vertex_buffer_data.layout.GetVertexStrideInBytes(), vertex_buffer_data.byteSize);

    return vertex_buffer_view;
}
