#pragma once
#include <memory>

#include "IRHIGPUBuffer.h"
#include "IRHIVertexBufferView.h"
#include "glTFScene/glTFScenePrimitive.h"

class IRHIVertexBuffer : public IRHIResource
{
public:
    IRHIVertexBuffer();
    
    virtual std::shared_ptr<IRHIVertexBufferView> CreateVertexBufferView(IRHIDevice& device, IRHICommandList& command_list,
        const RHIBufferDesc& desc, const VertexBufferData& vertex_buffer_data) = 0;

protected:
    std::shared_ptr<IRHIGPUBuffer> m_buffer;
    std::shared_ptr<IRHIGPUBuffer> m_upload_buffer;
};
