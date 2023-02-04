#pragma once
#include <d3d12.h>
#include <dxgiformat.h>

#include "../RHIUtils.h"
#include "../../glTFUtils/glTFLog.h"

enum class RHIBufferType;
enum class RHIDataFormat;

#define SAFE_RELEASE(x) if ((x)) {(x)->Release(); (x) = nullptr; LOG_FORMAT_FLUSH("[Exit] Release %s\n", #x)}

class DX12ConverterUtils
{
public:
    static DXGI_FORMAT ConvertToDXGIFormat(RHIDataFormat format);
    static D3D12_HEAP_TYPE ConvertToHeapType(RHIBufferType type);
    static D3D12_RESOURCE_STATES ConvertToResourceState(RHIResourceStateType state);
    static D3D_PRIMITIVE_TOPOLOGY ConvertToPrimitiveTopologyType(RHIPrimitiveTopologyType type);
    static D3D12_DESCRIPTOR_HEAP_TYPE ConvertToDescriptorHeapType(RHIDescriptorHeapType type);
    static D3D12_SRV_DIMENSION ConvertToSRVDimensionType(RHIShaderVisibleViewDimension type);
};

class DX12Utils : public RHIUtils
{
    friend class RHIResourceFactory;
public:
    virtual ~DX12Utils() override;
    
    // get the number of bits per pixel for a dxgi format
    static int GetDXGIFormatBitsPerPixel(DXGI_FORMAT& dxgiFormat);
    
    virtual bool ResetCommandList(IRHICommandList& commandList, IRHICommandAllocator& commandAllocator) override;
    virtual bool ResetCommandList(IRHICommandList& commandList, IRHICommandAllocator& commandAllocator, IRHIPipelineStateObject& initPSO) override;
    virtual bool CloseCommandList(IRHICommandList& commandList) override;
    virtual bool ExecuteCommandList(IRHICommandList& commandList, IRHICommandQueue& commandQueue) override;
    virtual bool ResetCommandAllocator(IRHICommandAllocator& commandAllocator) override;

    virtual bool SetRootSignature(IRHICommandList& commandList, IRHIRootSignature& rootSignature) override;
    virtual bool SetViewport(IRHICommandList& commandList, const RHIViewportDesc& viewportDesc) override;
    virtual bool SetScissorRect(IRHICommandList& commandList, const RHIScissorRectDesc& scissorRect) override;

    virtual bool SetVertexBufferView(IRHICommandList& commandList, IRHIVertexBufferView& view) override;
    virtual bool SetIndexBufferView(IRHICommandList& commandList, IRHIIndexBufferView& view) override;
    virtual bool SetPrimitiveTopology(IRHICommandList& commandList, RHIPrimitiveTopologyType type) override;

    virtual bool SetDescriptorHeap(IRHICommandList& commandList, IRHIDescriptorHeap* descriptorArray, size_t descriptorCount) override;
    virtual bool SetGPUHandleToRootParameterSlot(IRHICommandList& commandList, unsigned slotIndex, RHIGPUDescriptorHandle handle) override;
    
    virtual bool UploadBufferDataToDefaultGPUBuffer(IRHICommandList& commandList, IRHIGPUBuffer& uploadBuffer, IRHIGPUBuffer& defaultBuffer, void* data, size_t size) override;
    virtual bool UploadTextureDataToDefaultGPUBuffer(IRHICommandList& commandList, IRHIGPUBuffer& uploadBuffer, IRHIGPUBuffer& defaultBuffer, void* data, size_t rowPitch, size_t slicePitch) override;
    
    virtual bool AddBufferBarrierToCommandList(IRHICommandList& commandList, IRHIGPUBuffer& buffer, RHIResourceStateType beforeState, RHIResourceStateType afterState) override;
    virtual bool AddRenderTargetBarrierToCommandList(IRHICommandList& commandList, IRHIRenderTarget& buffer, RHIResourceStateType beforeState, RHIResourceStateType afterState) override;
    virtual bool CreateConstantBufferViewInDescriptorHeap(IRHIDevice& device, IRHIDescriptorHeap& descriptorHeap, unsigned descriptorOffset, IRHIGPUBuffer& buffer, const RHIConstantBufferViewDesc& desc, RHIGPUDescriptorHandle& outGPUHandle) override;
    virtual bool CreateShaderResourceViewInDescriptorHeap(IRHIDevice& device, IRHIDescriptorHeap& descriptorHeap, unsigned descriptorOffset, IRHIGPUBuffer& buffer, const RHIShaderResourceViewDesc& desc, RHIGPUDescriptorHandle& outGPUHandle) override;
    
    virtual bool DrawIndexInstanced(IRHICommandList& commandList, unsigned indexCountPerInstance, unsigned instanceCount, unsigned startIndexLocation, unsigned baseVertexLocation, unsigned startInstanceLocation) override;
    virtual bool Present(IRHISwapChain& swapchain) override;
};
