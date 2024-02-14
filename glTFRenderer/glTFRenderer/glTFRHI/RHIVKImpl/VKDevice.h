#pragma once
#include "glTFRHI/RHIInterface/IRHIDevice.h"

class VKDevice : public IRHIDevice
{
public:
    virtual ~VKDevice() override;

    virtual bool InitDevice(IRHIFactory& factory) override;
};
