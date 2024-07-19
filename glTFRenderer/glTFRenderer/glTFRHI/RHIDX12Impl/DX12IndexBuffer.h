#pragma once
#include "glTFRHI/RHIInterface/IRHIIndexBuffer.h"

class DX12IndexBuffer : public IRHIIndexBuffer
{
public:
    virtual std::shared_ptr<IRHIIndexBufferView> CreateIndexBufferView(IRHIDevice& device, IRHIMemoryManager& memory_manager,
        IRHICommandList& command_list, const RHIBufferDesc& desc, const IndexBufferData& index_buffer_data) override;
};
