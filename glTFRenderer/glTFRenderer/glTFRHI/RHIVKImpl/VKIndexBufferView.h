#pragma once
#include "glTFRHI/RHIInterface/IRHIIndexBufferView.h"

class VKIndexBufferView : public IRHIIndexBufferView
{
public:
    virtual bool InitIndexBufferView(IRHIGPUBuffer& buffer, size_t offset, RHIDataFormat indexFormat, size_t indexBufferSize) override;
    
    virtual RHIGPUDescriptorHandle GetGPUHandle() const override;
    virtual size_t GetSize() const override;
    virtual unsigned GetFormat() const override;
};
