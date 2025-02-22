#pragma once
#include "IRHIBuffer.h"
#include "IRHIIndexBufferView.h"
#include "IRHIMemoryManager.h"
#include "glTFScene/glTFScenePrimitive.h"

class RHIIndexBuffer final
{
public:
    DECLARE_NON_COPYABLE_AND_DEFAULT_CTOR_VDTOR(RHIIndexBuffer)
    
    std::shared_ptr<IRHIIndexBufferView> CreateIndexBufferView(
        IRHIDevice& device,
        IRHIMemoryManager& memory_manager,
        IRHICommandList& command_list,
        const RHIBufferDesc& desc,
        const IndexBufferData& index_buffer_data);

    RHIDataFormat GetFormat() const {return m_index_format; }
    size_t GetCount() const {return m_index_count; }
    IRHIBuffer& GetBuffer() const {return *m_buffer->m_buffer; }
    
protected:
    std::shared_ptr<IRHIBufferAllocation> m_buffer {nullptr};
    std::shared_ptr<IRHIBufferAllocation> m_upload_buffer {nullptr};

    RHIDataFormat m_index_format {RHIDataFormat::UNKNOWN};
    size_t m_index_count {0};
};
