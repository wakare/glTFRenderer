#pragma once
#include <d3d12.h>
#include <dxgiformat.h>

#include "../RHIInterface/IRHIBarrier.h"
#include "../RHIUtils.h"

enum class RHIBufferType;
enum class RHIDataFormat;

class DX12ConverterUtils
{
public:
    static DXGI_FORMAT ConvertToDXGIFormat(RHIDataFormat format);
    static D3D12_HEAP_TYPE ConvertToHeapType(RHIBufferType type);
    static D3D12_RESOURCE_STATES ConvertToResourceState(RHIResourceStateType state);
};

class DX12Utils : public RHIUtils
{
    friend class RHIResourceFactory;
public:
    virtual bool ResetCommandList(IRHICommandList& commandList, IRHICommandAllocator& commandAllocator) override;
    virtual bool ResetCommandList(IRHICommandList& commandList, IRHICommandAllocator& commandAllocator, IRHIPipelineStateObject& initPSO) override;
    virtual bool CloseCommandList(IRHICommandList& commandList) override;
    virtual bool UploadDataToDefaultGPUBuffer(IRHICommandList& commandList, IRHIGPUBuffer& uploadBuffer, IRHIGPUBuffer& defaultBuffer, void* data, size_t size) override;
};
