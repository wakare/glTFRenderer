#include "DX12Utils.h"
#include <cassert>

#include "d3dx12.h"
#include "DX12CommandList.h"
#include "DX12GPUBuffer.h"
#include "DX12PipelineStateObject.h"
#include "../RHIInterface/IRHIGPUBuffer.h"
#include "../RHIInterface/RHICommon.h"

DXGI_FORMAT DX12ConverterUtils::ConvertToDXGIFormat(RHIDataFormat format)
{
    switch (format)
    {
    case RHIDataFormat::R8G8B8A8_UNORM:
        return DXGI_FORMAT_R8G8B8A8_UNORM;

    case RHIDataFormat::R8G8B8A8_UNORM_SRGB:
        return DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;

    case RHIDataFormat::R32G32B32_FLOAT:
        return DXGI_FORMAT_R32G32B32_FLOAT;

    case RHIDataFormat::R32G32B32A32_FLOAT:
        return DXGI_FORMAT_R32G32B32A32_FLOAT;
        
    case RHIDataFormat::Unknown: break;
    }
    
    assert(false);
    return DXGI_FORMAT_UNKNOWN;
}

D3D12_HEAP_TYPE DX12ConverterUtils::ConvertToHeapType(RHIBufferType type)
{
    switch (type)
    {
    case RHIBufferType::Default:
        return D3D12_HEAP_TYPE_DEFAULT;

    case RHIBufferType::Upload:
        return D3D12_HEAP_TYPE_UPLOAD;
    }

    assert(false);
    return D3D12_HEAP_TYPE_DEFAULT;
}

D3D12_RESOURCE_STATES DX12ConverterUtils::ConvertToResourceState(RHIResourceStateType state)
{
    switch (state)
    {
    case RHIResourceStateType::COPY_DEST:
        return D3D12_RESOURCE_STATE_COPY_DEST;
        
    case RHIResourceStateType::VERTEX_AND_CONSTANT_BUFFER:
        return D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER;
    }

    assert(false);
    return D3D12_RESOURCE_STATE_COMMON;
}

bool DX12Utils::ResetCommandList(IRHICommandList& commandList, IRHICommandAllocator& commandAllocator)
{
    auto* dxCommandList = dynamic_cast<DX12CommandList&>(commandList).GetCommandList();
    auto* dxCommandAllocator = dynamic_cast<DX12CommandAllocator&>(commandAllocator).GetCommandAllocator();

    dxCommandList->Reset(dxCommandAllocator, nullptr);
    return true;
}

bool DX12Utils::ResetCommandList(IRHICommandList& commandList, IRHICommandAllocator& commandAllocator,
    IRHIPipelineStateObject& initPSO)
{
    auto* dxCommandList = dynamic_cast<DX12CommandList&>(commandList).GetCommandList();
    auto* dxCommandAllocator = dynamic_cast<DX12CommandAllocator&>(commandAllocator).GetCommandAllocator();
    auto* dxPSO = dynamic_cast<DX12GraphicsPipelineStateObject&>(initPSO).GetPSO();
    
    dxCommandList->Reset(dxCommandAllocator, dxPSO);
    return true;
}

bool DX12Utils::CloseCommandList(IRHICommandList& commandList)
{
    auto* dxCommandList = dynamic_cast<DX12CommandList&>(commandList).GetCommandList();
    dxCommandList->Close();
    
    return true;
}

bool DX12Utils::UploadDataToDefaultGPUBuffer(IRHICommandList& commandList, IRHIGPUBuffer& uploadBuffer,
                                             IRHIGPUBuffer& defaultBuffer, void* data, size_t size)
{
    auto* dxCommandList = dynamic_cast<DX12CommandList&>(commandList).GetCommandList();
    auto* dxUploadBuffer = dynamic_cast<DX12GPUBuffer&>(uploadBuffer).GetBuffer();
    auto* dxDefaultBuffer = dynamic_cast<DX12GPUBuffer&>(defaultBuffer).GetBuffer();
    
    // store vertex buffer in upload heap
    D3D12_SUBRESOURCE_DATA vertexData = {};
    vertexData.pData = reinterpret_cast<BYTE*>(data); // pointer to our vertex array
    vertexData.RowPitch = size; // size of all our triangle vertex data
    vertexData.SlicePitch = size; // also the size of our triangle vertex data

    // we are now creating a command with the command list to copy the data from
    // the upload heap to the default heap
    UpdateSubresources(dxCommandList, dxDefaultBuffer, dxUploadBuffer, 0, 0, 1, &vertexData);

    return true;
}
