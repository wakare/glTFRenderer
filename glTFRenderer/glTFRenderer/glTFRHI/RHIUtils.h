#pragma once
#include <memory>

#include "RHIInterface/IRHICommandAllocator.h"
#include "RHIInterface/IRHIPipelineStateObject.h"
#include "RHIInterface/IRHIResource.h"

class IRHIGPUBuffer;
class IRHICommandList;

// Singleton for provide combined basic rhi operations
class RHIUtils : public IRHIResource
{
    friend class RHIResourceFactory;
public:
    virtual bool ResetCommandList(IRHICommandList& commandList, IRHICommandAllocator& commandAllocator) = 0;
    virtual bool ResetCommandList(IRHICommandList& commandList, IRHICommandAllocator& commandAllocator, IRHIPipelineStateObject& initPSO) = 0;
    virtual bool CloseCommandList(IRHICommandList& commandList) = 0;
    virtual bool UploadDataToDefaultGPUBuffer(IRHICommandList& commandList, IRHIGPUBuffer& uploadBuffer, IRHIGPUBuffer& defaultBuffer, void* data, size_t size) = 0;
    
    static RHIUtils& Instance();
    
protected:
    RHIUtils() = default;

    static std::shared_ptr<RHIUtils> g_instance;
};
