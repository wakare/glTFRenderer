#pragma once

class IRHIDescriptorUpdater
{
public:
    virtual bool BindBufferDescriptor() = 0;
    virtual bool BindTextureDescriptor() = 0;
};
