#pragma once
#include <memory>

#include "RHIInterface/IRHIBuffer.h"
#include "RHIInterface/IRHIVertexBufferView.h"
#include "RHIInterface/IRHIMemoryManager.h"

class RHICORE_API RHIVertexBuffer final
{
public:
    IMPL_NON_COPYABLE_AND_DEFAULT_CTOR_VDTOR(RHIVertexBuffer)
    
    std::shared_ptr<IRHIVertexBufferView> CreateVertexBufferView(
        IRHIDevice& device,
        IRHIMemoryManager& memory_manager,
        IRHICommandList& command_list,
        const RHIBufferDesc& desc,
        const VertexBufferData& vertex_buffer_data);

    const VertexLayoutDeclaration& GetLayout() const {return m_vertex_layout; }
    size_t GetCount() const {return m_vertex_count; }
    IRHIBuffer& GetBuffer() const {return *m_buffer->m_buffer; }
    
protected:
    std::shared_ptr<IRHIBufferAllocation> m_buffer {nullptr};
    std::shared_ptr<IRHIBufferAllocation> m_upload_buffer {nullptr};

    VertexLayoutDeclaration m_vertex_layout {};
    size_t m_vertex_count {0};
};
