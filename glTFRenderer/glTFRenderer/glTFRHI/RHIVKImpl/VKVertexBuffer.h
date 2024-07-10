#pragma once
#include "glTFRHI/RHIInterface/IRHIVertexBuffer.h"

class VKVertexBuffer : public IRHIVertexBuffer
{
public:
    virtual std::shared_ptr<IRHIVertexBufferView> CreateVertexBufferView(IRHIDevice& device, glTFRenderResourceManager& resource_manager,
                                                                         IRHICommandList& command_list, const RHIBufferDesc& desc, const VertexBufferData& vertex_buffer_data) override;
};
