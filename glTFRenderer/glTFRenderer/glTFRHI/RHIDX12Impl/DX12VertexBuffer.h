#pragma once
#include "glTFRHI/RHIInterface/IRHIVertexBuffer.h"

class DX12VertexBuffer : public IRHIVertexBuffer
{
public:
    virtual std::shared_ptr<IRHIVertexBufferView> CreateVertexBufferView(IRHIDevice& device, IRHIMemoryManager& memory_manager,
                                                                         IRHICommandList& command_list, const RHIBufferDesc& desc, const VertexBufferData& vertex_buffer_data) override;
};
