#pragma once
#include "glTFRHI/RHIInterface/IRHIIndexBuffer.h"

class VKIndexBuffer : public IRHIIndexBuffer
{
public:
    virtual std::shared_ptr<IRHIIndexBufferView> CreateIndexBufferView(IRHIDevice& device, IRHICommandList& command_list,
                                                                       glTFRenderResourceManager& resource_manager, const RHIBufferDesc& desc, const IndexBufferData& index_buffer_data) override;
    
};
