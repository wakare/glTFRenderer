﻿#pragma once
#include "IRHIGPUBuffer.h"
#include "IRHIIndexBufferView.h"
#include "glTFScene/glTFScenePrimitive.h"

class IRHIIndexBuffer : public IRHIResource
{
public:
    DECLARE_NON_COPYABLE_AND_DEFAULT_CTOR_VDTOR(IRHIIndexBuffer)
    
    virtual std::shared_ptr<IRHIIndexBufferView> CreateIndexBufferView(IRHIDevice& device, IRHICommandList& command_list,
        const RHIBufferDesc& desc, const IndexBufferData& index_buffer_data) = 0;

    RHIDataFormat GetFormat() const {return m_index_format; }
    size_t GetCount() const {return m_index_count; }
    IRHIGPUBuffer& GetBuffer() const {return *m_buffer; }
    
protected:
    std::shared_ptr<IRHIGPUBuffer> m_buffer {nullptr};
    std::shared_ptr<IRHIGPUBuffer> m_upload_buffer {nullptr};

    RHIDataFormat m_index_format {RHIDataFormat::Unknown};
    size_t m_index_count {0};
};
