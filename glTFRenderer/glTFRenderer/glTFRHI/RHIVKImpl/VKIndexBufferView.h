#pragma once
#include "glTFRHI/RHIInterface/IRHIIndexBufferView.h"

class VKIndexBufferView : public IRHIIndexBufferView
{
public:
    virtual bool InitIndexBufferView(IRHIBuffer& buffer, const RHIIndexBufferViewDesc& desc) override;
    
    virtual RHIGPUDescriptorHandle GetGPUHandle() const override;
    virtual size_t GetSize() const override;
    virtual unsigned GetFormat() const override;
};
