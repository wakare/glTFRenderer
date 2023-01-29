#include "DX12Utils.h"
#include <cassert>

#include "d3dx12.h"
#include "DX12CommandList.h"
#include "DX12CommandQueue.h"
#include "DX12GPUBuffer.h"
#include "DX12IndexBufferView.h"
#include "DX12PipelineStateObject.h"
#include "DX12RenderTarget.h"
#include "DX12RootSignature.h"
#include "DX12SwapChain.h"
#include "DX12VertexBufferView.h"
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
        
    case RHIDataFormat::D32_FLOAT:
        return DXGI_FORMAT_D32_FLOAT;

    case RHIDataFormat::R32_UINT:
        return DXGI_FORMAT_R32_UINT;
        
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

bool DX12Utils::SetViewport(IRHICommandList& commandList, const RHIViewportDesc& viewportDesc)
{
    auto* dxCommandList = dynamic_cast<DX12CommandList&>( commandList).GetCommandList();

    D3D12_VIEWPORT viewport = {viewportDesc.TopLeftX, viewportDesc.TopLeftY, viewportDesc.Width, viewportDesc.Height, viewportDesc.MinDepth, viewportDesc.MaxDepth};
    dxCommandList->RSSetViewports(1, &viewport);
    
    return true;
}

bool DX12Utils::SetScissorRect(IRHICommandList& commandList, const RHIScissorRectDesc& scissorRect)
{
    auto* dxCommandList = dynamic_cast<DX12CommandList&>( commandList).GetCommandList();
    D3D12_RECT dxScissorRect = {scissorRect.left, scissorRect.top, scissorRect.right, scissorRect.right};
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

bool DX12Utils::AddRenderTargetBarrierToCommandList(IRHICommandList& commandList, IRHIRenderTarget& renderTarget,
    RHIResourceStateType beforeState, RHIResourceStateType afterState)
{
    auto* dxCommandList = dynamic_cast<DX12CommandList&>(commandList).GetCommandList();
    auto* dxRenderTarget = dynamic_cast<DX12RenderTarget&>(renderTarget).GetRenderTarget();

    const CD3DX12_RESOURCE_BARRIER TransitionToVertexBufferState = CD3DX12_RESOURCE_BARRIER::Transition(dxRenderTarget,
        DX12ConverterUtils::ConvertToResourceState(beforeState), DX12ConverterUtils::ConvertToResourceState(afterState)); 
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
