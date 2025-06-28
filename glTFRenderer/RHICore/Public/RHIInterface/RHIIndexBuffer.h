#pragma once
#include "RHIInterface/IRHIBuffer.h"
#include "RHIInterface/IRHIIndexBufferView.h"

class IRHIBufferAllocation;
class IRHIMemoryManager;

class RHICORE_API RHIIndexBuffer final
{
public:
    IMPL_NON_COPYABLE_AND_DEFAULT_CTOR_VDTOR(RHIIndexBuffer)
    
    std::shared_ptr<IRHIIndexBufferView> CreateIndexBufferView(
        IRHIDevice& device,
        IRHIMemoryManager& memory_manager,
        IRHICommandList& command_list,
        const RHIBufferDesc& desc,
        const IndexBufferData& index_buffer_data);

    RHIDataFormat GetFormat() const;
    size_t GetCount() const;
    IRHIBuffer& GetBuffer() const;
    
protected:
    std::shared_ptr<IRHIBufferAllocation> m_buffer {nullptr};
    std::shared_ptr<IRHIBufferAllocation> m_upload_buffer {nullptr};

    RHIDataFormat m_index_format {RHIDataFormat::UNKNOWN};
    size_t m_index_count {0};
};
