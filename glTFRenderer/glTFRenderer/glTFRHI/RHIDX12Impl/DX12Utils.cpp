#include "DX12Utils.h"
#include <cassert>

#include "d3dx12.h"
#include "DX12CommandList.h"
#include "DX12CommandQueue.h"
#include "DX12DescriptorHeap.h"
#include "DX12Device.h"
#include "DX12GPUBuffer.h"
#include "DX12IndexBufferView.h"
#include "DX12PipelineStateObject.h"
#include "DX12RenderTarget.h"
#include "DX12RootSignature.h"
#include "DX12SwapChain.h"
#include "DX12VertexBufferView.h"
#include "../RHIInterface/IRHIGPUBuffer.h"
#include "../RHIInterface/RHICommon.h"

#define CONVERT_DXGI_FORMAT_CASE(RHIFormat) case RHIDataFormat::##RHIFormat: return DXGI_FORMAT_##RHIFormat;

DXGI_FORMAT DX12ConverterUtils::ConvertToDXGIFormat(RHIDataFormat format)
{
    switch (format)
    {
        CONVERT_DXGI_FORMAT_CASE(R8G8B8A8_UNORM)
        CONVERT_DXGI_FORMAT_CASE(R8G8B8A8_UNORM_SRGB)
        CONVERT_DXGI_FORMAT_CASE(B8G8R8A8_UNORM)
        CONVERT_DXGI_FORMAT_CASE(B8G8R8X8_UNORM)
        CONVERT_DXGI_FORMAT_CASE(R32G32_FLOAT)
        CONVERT_DXGI_FORMAT_CASE(R32G32B32_FLOAT)
        CONVERT_DXGI_FORMAT_CASE(R32G32B32A32_FLOAT)
        CONVERT_DXGI_FORMAT_CASE(R16G16B16A16_FLOAT)
        CONVERT_DXGI_FORMAT_CASE(R16G16B16A16_UNORM)
        CONVERT_DXGI_FORMAT_CASE(R10G10B10A2_UNORM)
        CONVERT_DXGI_FORMAT_CASE(R10G10B10_XR_BIAS_A2_UNORM)
        CONVERT_DXGI_FORMAT_CASE(B5G5R5A1_UNORM)
        CONVERT_DXGI_FORMAT_CASE(B5G6R5_UNORM)
        CONVERT_DXGI_FORMAT_CASE(D32_FLOAT)
        CONVERT_DXGI_FORMAT_CASE(R32_FLOAT)
        CONVERT_DXGI_FORMAT_CASE(R32_UINT)
        CONVERT_DXGI_FORMAT_CASE(R32_TYPELESS)
        CONVERT_DXGI_FORMAT_CASE(R16_FLOAT)
        CONVERT_DXGI_FORMAT_CASE(R16_UNORM)
        CONVERT_DXGI_FORMAT_CASE(R16_UINT)
        CONVERT_DXGI_FORMAT_CASE(R8_UNORM)
        CONVERT_DXGI_FORMAT_CASE(A8_UNORM)    
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

    case RHIResourceStateType::INDEX_BUFFER:
        return D3D12_RESOURCE_STATE_INDEX_BUFFER;

    case RHIResourceStateType::PRESENT:
        return D3D12_RESOURCE_STATE_PRESENT;

    case RHIResourceStateType::RENDER_TARGET:
        return D3D12_RESOURCE_STATE_RENDER_TARGET;

    case RHIResourceStateType::DEPTH_WRITE:
        return D3D12_RESOURCE_STATE_DEPTH_WRITE;

    case RHIResourceStateType::DEPTH_READ:
        return D3D12_RESOURCE_STATE_DEPTH_READ;
        
    case RHIResourceStateType::PIXEL_SHADER_RESOURCE:
        return D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
    }

    assert(false);
    return D3D12_RESOURCE_STATE_COMMON;
}

D3D_PRIMITIVE_TOPOLOGY DX12ConverterUtils::ConvertToPrimitiveTopologyType(RHIPrimitiveTopologyType type)
{
    switch (type)
    {
        case RHIPrimitiveTopologyType::TRIANGLELIST:
            return D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
    }

    assert(false);
    return D3D_PRIMITIVE_TOPOLOGY_UNDEFINED;
}

D3D12_DESCRIPTOR_HEAP_TYPE DX12ConverterUtils::ConvertToDescriptorHeapType(RHIDescriptorHeapType type)
{
    switch (type) {
        case RHIDescriptorHeapType::CBV_SRV_UAV:
            return D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
        
        case RHIDescriptorHeapType::SAMPLER:
            return D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER;
        
        case RHIDescriptorHeapType::RTV:
            return D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
        
        case RHIDescriptorHeapType::DSV:
            return D3D12_DESCRIPTOR_HEAP_TYPE_DSV;
    }

    assert(false);
    return D3D12_DESCRIPTOR_HEAP_TYPE_NUM_TYPES;
}

#define COVERT_TO_SRV_DIMENSION_CASE(dimensionType) case RHIShaderVisibleViewDimension::##dimensionType: return D3D12_SRV_DIMENSION_##dimensionType;

D3D12_SRV_DIMENSION DX12ConverterUtils::ConvertToSRVDimensionType(RHIShaderVisibleViewDimension type)
{
    switch (type) {
        COVERT_TO_SRV_DIMENSION_CASE(BUFFER)
        COVERT_TO_SRV_DIMENSION_CASE(TEXTURE1D)
        COVERT_TO_SRV_DIMENSION_CASE(TEXTURE1DARRAY)
        COVERT_TO_SRV_DIMENSION_CASE(TEXTURE2D)
        COVERT_TO_SRV_DIMENSION_CASE(TEXTURE2DARRAY)
        COVERT_TO_SRV_DIMENSION_CASE(TEXTURE2DMS)
        COVERT_TO_SRV_DIMENSION_CASE(TEXTURE2DMSARRAY)
        COVERT_TO_SRV_DIMENSION_CASE(TEXTURE3D)
        COVERT_TO_SRV_DIMENSION_CASE(TEXTURECUBE)
        COVERT_TO_SRV_DIMENSION_CASE(TEXTURECUBEARRAY)
    }

    return D3D12_SRV_DIMENSION_UNKNOWN;
}

DX12Utils::~DX12Utils()
{
}

int DX12Utils::GetDXGIFormatBitsPerPixel(DXGI_FORMAT& dxgiFormat)
{
    if (dxgiFormat == DXGI_FORMAT_R32G32B32A32_FLOAT) return 128;
    else if (dxgiFormat == DXGI_FORMAT_R16G16B16A16_FLOAT) return 64;
    else if (dxgiFormat == DXGI_FORMAT_R16G16B16A16_UNORM) return 64;
    else if (dxgiFormat == DXGI_FORMAT_R8G8B8A8_UNORM) return 32;
    else if (dxgiFormat == DXGI_FORMAT_B8G8R8A8_UNORM) return 32;
    else if (dxgiFormat == DXGI_FORMAT_B8G8R8X8_UNORM) return 32;
    else if (dxgiFormat == DXGI_FORMAT_R10G10B10_XR_BIAS_A2_UNORM) return 32;

    else if (dxgiFormat == DXGI_FORMAT_R10G10B10A2_UNORM) return 32;
    else if (dxgiFormat == DXGI_FORMAT_B5G5R5A1_UNORM) return 16;
    else if (dxgiFormat == DXGI_FORMAT_B5G6R5_UNORM) return 16;
    else if (dxgiFormat == DXGI_FORMAT_R32_FLOAT) return 32;
    else if (dxgiFormat == DXGI_FORMAT_R16_FLOAT) return 16;
    else if (dxgiFormat == DXGI_FORMAT_R16_UNORM) return 16;
    else if (dxgiFormat == DXGI_FORMAT_R8_UNORM) return 8;
    else if (dxgiFormat == DXGI_FORMAT_A8_UNORM) return 8;

    // Unknown format !
    assert(false);
    return 32;
}

bool DX12Utils::ResetCommandList(IRHICommandList& commandList, IRHICommandAllocator& commandAllocator,
                                 IRHIPipelineStateObject* initPSO)
{
    auto* dxCommandList = dynamic_cast<DX12CommandList&>(commandList).GetCommandList();
    auto* dxCommandAllocator = dynamic_cast<DX12CommandAllocator&>(commandAllocator).GetCommandAllocator();
    auto* dxPSO = initPSO ? dynamic_cast<DX12GraphicsPipelineStateObject&>(*initPSO).GetPSO() : nullptr;
    
    dxCommandList->Reset(dxCommandAllocator, dxPSO);
    return true;
}

bool DX12Utils::CloseCommandList(IRHICommandList& commandList)
{
    auto* dxCommandList = dynamic_cast<DX12CommandList&>(commandList).GetCommandList();
    dxCommandList->Close();
    
    return true;
}

bool DX12Utils::ExecuteCommandList(IRHICommandList& commandList, IRHICommandQueue& commandQueue)
{
    auto* dxCommandList = dynamic_cast<DX12CommandList&>(commandList).GetCommandList();
    auto* dxCommandQueue = dynamic_cast<DX12CommandQueue&>(commandQueue).GetCommandQueue();
    
    ID3D12CommandList* ppCommandLists[] = { dxCommandList };
    dxCommandQueue->ExecuteCommandLists(_countof(ppCommandLists), ppCommandLists);
    
    return true;
}

bool DX12Utils::ResetCommandAllocator(IRHICommandAllocator& commandAllocator)
{
    auto* dxCommandAllocator = dynamic_cast<DX12CommandAllocator&>(commandAllocator).GetCommandAllocator();
    THROW_IF_FAILED(dxCommandAllocator->Reset())
    return true;
}

bool DX12Utils::SetRootSignature(IRHICommandList& commandList, IRHIRootSignature& rootSignature)
{
    auto* dxCommandList = dynamic_cast<DX12CommandList&>(commandList).GetCommandList();
    auto* dxRootSignature = dynamic_cast<DX12RootSignature&>(rootSignature).GetRootSignature();
    
    dxCommandList->SetGraphicsRootSignature(dxRootSignature);
    
    return true;
}

bool DX12Utils::SetViewport(IRHICommandList& commandList, const RHIViewportDesc& viewport_desc)
{
    auto* dxCommandList = dynamic_cast<DX12CommandList&>( commandList).GetCommandList();

    D3D12_VIEWPORT viewport = {viewport_desc.TopLeftX, viewport_desc.TopLeftY, viewport_desc.Width, viewport_desc.Height, viewport_desc.MinDepth, viewport_desc.MaxDepth};
    dxCommandList->RSSetViewports(1, &viewport);
    
    return true;
}

bool DX12Utils::SetScissorRect(IRHICommandList& commandList, const RHIScissorRectDesc& scissor_rect)
{
    auto* dxCommandList = dynamic_cast<DX12CommandList&>( commandList).GetCommandList();
    D3D12_RECT dxScissorRect = {scissor_rect.left, scissor_rect.top, scissor_rect.right, scissor_rect.right};
    dxCommandList->RSSetScissorRects(1, &dxScissorRect);
    
    return true;
}

bool DX12Utils::SetVertexBufferView(IRHICommandList& commandList, IRHIVertexBufferView& view)
{
    auto* dxCommandList = dynamic_cast<DX12CommandList&>(commandList).GetCommandList();
    auto& dxVertexBufferView = dynamic_cast<DX12VertexBufferView&>(view).GetVertexBufferView();

    dxCommandList->IASetVertexBuffers(0, 1, &dxVertexBufferView);
    
    return true;
}

bool DX12Utils::SetIndexBufferView(IRHICommandList& commandList, IRHIIndexBufferView& view)
{
    auto* dxCommandList = dynamic_cast<DX12CommandList&>(commandList).GetCommandList();
    auto& dxIndexBufferView = dynamic_cast<DX12IndexBufferView&>(view).GetIndexBufferView();

    dxCommandList->IASetIndexBuffer(&dxIndexBufferView);
    
    return true;
}

bool DX12Utils::SetPrimitiveTopology(IRHICommandList& commandList, RHIPrimitiveTopologyType type)
{
    auto* dxCommandList = dynamic_cast<DX12CommandList&>(commandList).GetCommandList();
    dxCommandList->IASetPrimitiveTopology(DX12ConverterUtils::ConvertToPrimitiveTopologyType(type));
    
    return true;
}

bool DX12Utils::SetDescriptorHeapArray(IRHICommandList& commandList, IRHIDescriptorHeap* descriptor_heap_array_data,
                                  size_t descriptor_heap_array_count)
{
    auto* dxCommandList = dynamic_cast<DX12CommandList&>(commandList).GetCommandList();

    std::vector<ID3D12DescriptorHeap*> dx_descriptor_heaps;
    for (size_t i = 0; i < descriptor_heap_array_count; ++i)
    {
        auto* dx_descriptor_heap = dynamic_cast<DX12DescriptorHeap&>(descriptor_heap_array_data[i]).GetDescriptorHeap();
        dx_descriptor_heaps.push_back(dx_descriptor_heap);
    }
    
    dxCommandList->SetDescriptorHeaps(dx_descriptor_heaps.size(), dx_descriptor_heaps.data());
    
    return true;
}

bool DX12Utils::SetConstantBufferViewGPUHandleToRootParameterSlot(IRHICommandList& commandList, unsigned slotIndex,
    RHIGPUDescriptorHandle handle)
{
    auto* dxCommandList = dynamic_cast<DX12CommandList&>(commandList).GetCommandList();
    dxCommandList->SetGraphicsRootConstantBufferView(slotIndex, handle);
    
    return true;
}

bool DX12Utils::SetShaderResourceViewGPUHandleToRootParameterSlot(IRHICommandList& commandList, unsigned slotIndex,
    RHIGPUDescriptorHandle handle)
{
    auto* dxCommandList = dynamic_cast<DX12CommandList&>(commandList).GetCommandList();
    dxCommandList->SetGraphicsRootShaderResourceView(slotIndex, handle);
    
    return true;
}

bool DX12Utils::SetDescriptorTableGPUHandleToRootParameterSlot(IRHICommandList& commandList, unsigned slotIndex,
                                                               RHIGPUDescriptorHandle handle)
{
    auto* dxCommandList = dynamic_cast<DX12CommandList&>(commandList).GetCommandList();

    D3D12_GPU_DESCRIPTOR_HANDLE dxHandle = {handle} ;
    dxCommandList->SetGraphicsRootDescriptorTable(slotIndex, dxHandle);
    
    return true;
}

bool DX12Utils::UploadBufferDataToDefaultGPUBuffer(IRHICommandList& commandList, IRHIGPUBuffer& uploadBuffer,
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

bool DX12Utils::UploadTextureDataToDefaultGPUBuffer(IRHICommandList& commandList, IRHIGPUBuffer& uploadBuffer,
    IRHIGPUBuffer& defaultBuffer, void* data, size_t rowPitch, size_t slicePitch)
{
    auto* dxCommandList = dynamic_cast<DX12CommandList&>(commandList).GetCommandList();
    auto* dxUploadBuffer = dynamic_cast<DX12GPUBuffer&>(uploadBuffer).GetBuffer();
    auto* dxDefaultBuffer = dynamic_cast<DX12GPUBuffer&>(defaultBuffer).GetBuffer();
    
    // store vertex buffer in upload heap
    D3D12_SUBRESOURCE_DATA vertexData = {};
    vertexData.pData = reinterpret_cast<BYTE*>(data); // pointer to our vertex array
    vertexData.RowPitch = rowPitch; // size of all our triangle vertex data
    vertexData.SlicePitch = slicePitch; // also the size of our triangle vertex data

    // we are now creating a command with the command list to copy the data from
    // the upload heap to the default heap
    UpdateSubresources(dxCommandList, dxDefaultBuffer, dxUploadBuffer, 0, 0, 1, &vertexData);

    return true;
}

bool DX12Utils::AddBufferBarrierToCommandList(IRHICommandList& commandList, IRHIGPUBuffer& buffer,
                                              RHIResourceStateType beforeState, RHIResourceStateType afterState)
{
    auto* dxCommandList = dynamic_cast<DX12CommandList&>(commandList).GetCommandList();
    auto* dxBuffer = dynamic_cast<DX12GPUBuffer&>(buffer).GetBuffer();
    
    CD3DX12_RESOURCE_BARRIER TransitionToVertexBufferState = CD3DX12_RESOURCE_BARRIER::Transition(dxBuffer,
        DX12ConverterUtils::ConvertToResourceState(beforeState), DX12ConverterUtils::ConvertToResourceState(afterState)); 
    dxCommandList->ResourceBarrier(1, &TransitionToVertexBufferState);

    return true;
}

bool DX12Utils::AddRenderTargetBarrierToCommandList(IRHICommandList& commandList, IRHIRenderTarget& render_target,
    RHIResourceStateType before_state, RHIResourceStateType after_state)
{
    auto* dxCommandList = dynamic_cast<DX12CommandList&>(commandList).GetCommandList();
    auto* dxRenderTarget = dynamic_cast<DX12RenderTarget&>(render_target).GetRenderTarget();

    const CD3DX12_RESOURCE_BARRIER TransitionToVertexBufferState = CD3DX12_RESOURCE_BARRIER::Transition(dxRenderTarget,
        DX12ConverterUtils::ConvertToResourceState(before_state), DX12ConverterUtils::ConvertToResourceState(after_state)); 
    dxCommandList->ResourceBarrier(1, &TransitionToVertexBufferState);

    return true;
}

bool DX12Utils::DrawIndexInstanced(IRHICommandList& commandList, unsigned indexCountPerInstance, unsigned instanceCount,
                                   unsigned startIndexLocation, unsigned baseVertexLocation, unsigned startInstanceLocation)
{
    auto* dxCommandList = dynamic_cast<DX12CommandList&>(commandList).GetCommandList();
    dxCommandList->DrawIndexedInstanced(indexCountPerInstance, instanceCount, startIndexLocation, static_cast<INT>(baseVertexLocation), startInstanceLocation);
    
    return true;
}

bool DX12Utils::Present(IRHISwapChain& swapchain)
{
    auto* dxSwapchain = dynamic_cast<DX12SwapChain&>(swapchain).GetSwapChain();
    THROW_IF_FAILED(dxSwapchain->Present(0, 0))
    return true;
}

bool DX12Utils::DiscardResource(IRHICommandList& commandList, IRHIRenderTarget& render_target)
{
    auto* dxCommandList = dynamic_cast<DX12CommandList&>(commandList).GetCommandList();
    auto* dxResource = dynamic_cast<DX12RenderTarget&>(render_target).GetResource();
    dxCommandList->DiscardResource(dxResource, nullptr);

    return true;
}
