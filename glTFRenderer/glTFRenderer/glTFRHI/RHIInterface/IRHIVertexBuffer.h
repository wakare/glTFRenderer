#pragma once
#include <memory>

#include "IRHIGPUBuffer.h"
#include "IRHIVertexBufferView.h"
#include "glTFScene/glTFScenePrimitive.h"

class IRHIVertexBuffer : public IRHIResource
{
public:
    DECLARE_NON_COPYABLE_AND_DEFAULT_CTOR_VDTOR(IRHIVertexBuffer)
    
    virtual std::shared_ptr<IRHIVertexBufferView> CreateVertexBufferView(IRHIDevice& device, IRHICommandList& command_list,
        const RHIBufferDesc& desc, const VertexBufferData& vertex_buffer_data) = 0;

    const VertexLayoutDeclaration& GetLayout() const {return m_vertex_layout; }
    size_t GetCount() const {return m_vertex_count; }
    IRHIGPUBuffer& GetBuffer() const {return *m_buffer; }
    
protected:
    std::shared_ptr<IRHIGPUBuffer> m_buffer {nullptr};
    std::shared_ptr<IRHIGPUBuffer> m_upload_buffer {nullptr};

    VertexLayoutDeclaration m_vertex_layout {};
    size_t m_vertex_count {0};
};
