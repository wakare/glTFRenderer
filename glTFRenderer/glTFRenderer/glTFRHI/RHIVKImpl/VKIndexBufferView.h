#pragma once
#include "glTFRHI/RHIInterface/IRHIIndexBufferView.h"

class VKIndexBufferView : public IRHIIndexBufferView
{
public:
    virtual bool InitIndexBufferView(IRHIBuffer& buffer, const RHIIndexBufferViewDesc& desc) override;
};
