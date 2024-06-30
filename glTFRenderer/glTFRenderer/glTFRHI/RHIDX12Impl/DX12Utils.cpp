#include "DX12Utils.h"

#include <imgui/imgui.h>
#include <imgui/backends/imgui_impl_dx12.h>

#include "d3dx12.h"
#include "DX12CommandList.h"
#include "DX12CommandQueue.h"
#include "DX12CommandSignature.h"
#include "DX12ConverterUtils.h"
#include "DX12DescriptorHeap.h"
#include "DX12Device.h"
#include "DX12Fence.h"
#include "DX12Buffer.h"
#include "DX12IndexBufferView.h"
#include "DX12PipelineStateObject.h"
#include "DX12RenderTarget.h"
#include "DX12RootSignature.h"
#include "DX12ShaderTable.h"
#include "DX12SwapChain.h"
#include "DX12VertexBufferView.h"
#include "glTFRHI/RHIInterface/IRHIFence.h"
#include "glTFRHI/RHIInterface/IRHIBuffer.h"
#include "glTFRHI/RHIInterface/RHICommon.h"

bool DX12Utils::InitGUIContext(IRHIDevice& device, IRHIDescriptorHeap& descriptor_heap, unsigned back_buffer_count)
{
    auto* dx_device = dynamic_cast<DX12Device&>(device).GetDevice();
    auto* dx_descriptor_heap = dynamic_cast<DX12DescriptorHeap&>(descriptor_heap).GetDescriptorHeap();
    
    ImGui_ImplDX12_Init(dx_device, back_buffer_count,
        DXGI_FORMAT_R8G8B8A8_UNORM, dx_descriptor_heap,
        dynamic_cast<DX12DescriptorHeap&>(descriptor_heap).GetCPUHandleForHeapStart(),
        dynamic_cast<DX12DescriptorHeap&>(descriptor_heap).GetGPUHandleForHeapStart());
    
    return true;
}

bool DX12Utils::NewGUIFrame()
{
    ImGui_ImplDX12_NewFrame();

    return true;
}

bool DX12Utils::RenderGUIFrame(IRHICommandList& commandList)
{
    auto* dx_command_list = dynamic_cast<DX12CommandList&>(commandList).GetCommandList();
    ImGui_ImplDX12_RenderDrawData(ImGui::GetDrawData(), dx_command_list);

    return true;
}

bool DX12Utils::ExitGUI()
{
    ImGui_ImplDX12_Shutdown();
    return true;
}

bool DX12Utils::BeginRenderPass(IRHICommandList& command_list, const RHIBeginRenderPassInfo& begin_render_pass_info)
{
    return true;
}

bool DX12Utils::EndRenderPass(IRHICommandList& command_list)
{
    return true;
}

bool DX12Utils::ResetCommandList(IRHICommandList& commandList, IRHICommandAllocator& commandAllocator,
                                 IRHIPipelineStateObject* initPSO)
{
    auto* dx_command_list = dynamic_cast<DX12CommandList&>(commandList).GetCommandList();
    auto* dx_command_allocator = dynamic_cast<DX12CommandAllocator&>(commandAllocator).GetCommandAllocator();
    if (initPSO)
    {
        const auto& dxPSO = dynamic_cast<IDX12PipelineStateObjectCommon&>(*initPSO);
        
        if (initPSO->GetPSOType() == RHIPipelineType::RayTracing)
        {
            auto* dxr_command_list = dynamic_cast<DX12CommandList&>(commandList).GetDXRCommandList();
            dxr_command_list->Reset(dx_command_allocator, nullptr);
            dxr_command_list->SetPipelineState1(dxPSO.GetDXRPipelineStateObject());
        }
        else
        {
            dx_command_list->Reset(dx_command_allocator, dxPSO.GetPipelineStateObject());    
        }
    }
    else
    {
        dx_command_list->Reset(dx_command_allocator, nullptr);    
    }
    
    return true;
}

bool DX12Utils::CloseCommandList(IRHICommandList& commandList)
{
    auto* dxCommandList = dynamic_cast<DX12CommandList&>(commandList).GetCommandList();
    dxCommandList->Close();
    
    return true;
}

bool DX12Utils::ExecuteCommandList(IRHICommandList& command_list, IRHICommandQueue& command_queue, const RHIExecuteCommandListContext& context)
{
    auto* dx_command_list = dynamic_cast<DX12CommandList&>(command_list).GetCommandList();
    auto* dx_command_queue = dynamic_cast<DX12CommandQueue&>(command_queue).GetCommandQueue();
    auto& fence = dynamic_cast<DX12CommandList&>(command_list).GetFence();
    
    ID3D12CommandList* ppCommandLists[] = { dx_command_list };
    dx_command_queue->ExecuteCommandLists(_countof(ppCommandLists), ppCommandLists);

    dynamic_cast<DX12Fence&>(fence).SignalWhenCommandQueueFinish(command_queue);
    
    return true;
}

bool DX12Utils::ResetCommandAllocator(IRHICommandAllocator& commandAllocator)
{
    auto* dxCommandAllocator = dynamic_cast<DX12CommandAllocator&>(commandAllocator).GetCommandAllocator();
    THROW_IF_FAILED(dxCommandAllocator->Reset())
    return true;
}

bool DX12Utils::WaitCommandListFinish(IRHICommandList& command_list)
{
    return command_list.WaitCommandList();
}

bool DX12Utils::SetRootSignature(IRHICommandList& commandList, IRHIRootSignature& rootSignature, bool isGraphicsPipeline)
{
    auto* dxCommandList = dynamic_cast<DX12CommandList&>(commandList).GetCommandList();
    auto* dxRootSignature = dynamic_cast<DX12RootSignature&>(rootSignature).GetRootSignature();

    if (isGraphicsPipeline)
    {
        dxCommandList->SetGraphicsRootSignature(dxRootSignature);    
    }
    else
    {
        dxCommandList->SetComputeRootSignature(dxRootSignature);
    }
    
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

bool DX12Utils::SetVertexBufferView(IRHICommandList& commandList, unsigned slot, IRHIVertexBufferView& view)
{
    auto* dxCommandList = dynamic_cast<DX12CommandList&>(commandList).GetCommandList();
    auto& dxVertexBufferView = dynamic_cast<DX12VertexBufferView&>(view).GetVertexBufferView();

    dxCommandList->IASetVertexBuffers(slot, 1, &dxVertexBufferView);
    
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

bool DX12Utils::SetConstant32BitToRootParameterSlot(IRHICommandList& commandList, unsigned slotIndex, unsigned* data,
                                                    unsigned count, bool isGraphicsPipeline)
{
    auto* dxCommandList = dynamic_cast<DX12CommandList&>(commandList).GetCommandList();
    if (isGraphicsPipeline)
    {
        for (unsigned i = 0; i < count; ++i)
        {
            dxCommandList->SetGraphicsRoot32BitConstant(slotIndex, data[count], count);    
        }
            
    }
    else
    {
        for (unsigned i = 0; i < count; ++i)
        {
            dxCommandList->SetComputeRoot32BitConstant(slotIndex, data[count], count);    
        }
    }
    
    return true;
}

bool DX12Utils::SetCBVToRootParameterSlot(IRHICommandList& commandList, unsigned slotIndex,
                                          RHIGPUDescriptorHandle handle, bool isGraphicsPipeline)
{
    auto* dxCommandList = dynamic_cast<DX12CommandList&>(commandList).GetCommandList();
    if (isGraphicsPipeline)
    {
        dxCommandList->SetGraphicsRootConstantBufferView(slotIndex, handle);    
    }
    else
    {
        dxCommandList->SetComputeRootConstantBufferView(slotIndex, handle);
    }
    
    return true;
}

bool DX12Utils::SetSRVToRootParameterSlot(IRHICommandList& commandList, unsigned slotIndex,
                                          RHIGPUDescriptorHandle handle, bool isGraphicsPipeline)
{
    auto* dxCommandList = dynamic_cast<DX12CommandList&>(commandList).GetCommandList();
    if (isGraphicsPipeline)
    {
        dxCommandList->SetGraphicsRootShaderResourceView(slotIndex, handle);    
    }
    else
    {
        dxCommandList->SetComputeRootShaderResourceView(slotIndex, handle);
    }
    
    return true;
}

bool DX12Utils::SetDTToRootParameterSlot(IRHICommandList& commandList, unsigned slotIndex,
                                         RHIGPUDescriptorHandle handle, bool isGraphicsPipeline)
{
    auto* dxCommandList = dynamic_cast<DX12CommandList&>(commandList).GetCommandList();

    D3D12_GPU_DESCRIPTOR_HANDLE dxHandle = {handle};
    if (isGraphicsPipeline)
    {
        dxCommandList->SetGraphicsRootDescriptorTable(slotIndex, dxHandle);    
    }
    else
    {
        dxCommandList->SetComputeRootDescriptorTable(slotIndex, dxHandle);
    }
    
    return true;
}

bool DX12Utils::UploadBufferDataToDefaultGPUBuffer(IRHICommandList& commandList, IRHIBuffer& uploadBuffer,
                                             IRHIBuffer& defaultBuffer, void* data, size_t size)
{
    auto* dxCommandList = dynamic_cast<DX12CommandList&>(commandList).GetCommandList();
    auto* dxUploadBuffer = dynamic_cast<DX12Buffer&>(uploadBuffer).GetBuffer();
    auto* dxDefaultBuffer = dynamic_cast<DX12Buffer&>(defaultBuffer).GetBuffer();
    
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

bool DX12Utils::UploadTextureDataToDefaultGPUBuffer(IRHICommandList& commandList, IRHIBuffer& uploadBuffer,
    IRHIBuffer& defaultBuffer, void* data, size_t rowPitch, size_t slicePitch)
{
    auto* dxCommandList = dynamic_cast<DX12CommandList&>(commandList).GetCommandList();
    auto* dxUploadBuffer = dynamic_cast<DX12Buffer&>(uploadBuffer).GetBuffer();
    auto* dxDefaultBuffer = dynamic_cast<DX12Buffer&>(defaultBuffer).GetBuffer();
    
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

bool DX12Utils::AddBufferBarrierToCommandList(IRHICommandList& commandList, IRHIBuffer& buffer,
                                              RHIResourceStateType beforeState, RHIResourceStateType afterState)
{
    auto* dxCommandList = dynamic_cast<DX12CommandList&>(commandList).GetCommandList();
    auto* dxBuffer = dynamic_cast<DX12Buffer&>(buffer).GetBuffer();
    
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

bool DX12Utils::DrawInstanced(IRHICommandList& commandList, unsigned vertexCountPerInstance, unsigned instanceCount,
    unsigned startVertexLocation, unsigned startInstanceLocation)
{
    auto* dxCommandList = dynamic_cast<DX12CommandList&>(commandList).GetCommandList();
    dxCommandList->DrawInstanced(vertexCountPerInstance, instanceCount, startVertexLocation, startInstanceLocation);
    
    return true;
}

bool DX12Utils::DrawIndexInstanced(IRHICommandList& commandList, unsigned indexCountPerInstance, unsigned instanceCount,
                                   unsigned startIndexLocation, unsigned baseVertexLocation, unsigned startInstanceLocation)
{
    auto* dxCommandList = dynamic_cast<DX12CommandList&>(commandList).GetCommandList();
    dxCommandList->DrawIndexedInstanced(indexCountPerInstance, instanceCount, startIndexLocation, static_cast<INT>(baseVertexLocation), startInstanceLocation);
    
    return true;
}

bool DX12Utils::Dispatch(IRHICommandList& command_list, unsigned X, unsigned Y, unsigned Z)
{
    auto* dxCommandList = dynamic_cast<DX12CommandList&>(command_list).GetCommandList();
    dxCommandList->Dispatch(X, Y, Z);

    return true;
}

bool DX12Utils::TraceRay(IRHICommandList& command_list, IRHIShaderTable& shader_table, unsigned X, unsigned Y, unsigned Z)
{
    auto* dxCommandList = dynamic_cast<DX12CommandList&>(command_list).GetDXRCommandList();
    const auto& dxShaderTable = dynamic_cast<DX12ShaderTable&>(shader_table);
    
    D3D12_DISPATCH_RAYS_DESC dispatch_rays_desc = {};
    dispatch_rays_desc.Width = X;
    dispatch_rays_desc.Height = Y;
    dispatch_rays_desc.Depth = Z;
    
    dispatch_rays_desc.HitGroupTable.StartAddress = dxShaderTable.GetHitGroupShaderTable()->GetGPUVirtualAddress();
    dispatch_rays_desc.HitGroupTable.SizeInBytes = dxShaderTable.GetHitGroupShaderTable()->GetDesc().Width;
    dispatch_rays_desc.HitGroupTable.StrideInBytes = dxShaderTable.GetHitGroupStride();

    dispatch_rays_desc.MissShaderTable.StartAddress = dxShaderTable.GetMissShaderTable()->GetGPUVirtualAddress();
    dispatch_rays_desc.MissShaderTable.SizeInBytes = dxShaderTable.GetMissShaderTable()->GetDesc().Width;
    dispatch_rays_desc.MissShaderTable.StrideInBytes = dxShaderTable.GetMissStride();

    dispatch_rays_desc.RayGenerationShaderRecord.StartAddress = dxShaderTable.GetRayGenShaderTable()->GetGPUVirtualAddress();
    dispatch_rays_desc.RayGenerationShaderRecord.SizeInBytes = dxShaderTable.GetRayGenShaderTable()->GetDesc().Width;

    dxCommandList->DispatchRays(&dispatch_rays_desc);

    return true;
}

bool DX12Utils::ExecuteIndirect(IRHICommandList& command_list, IRHICommandSignature& command_signature,
    unsigned max_count, IRHIBuffer& arguments_buffer, unsigned arguments_buffer_offset)
{
    auto* dx_command_list = dynamic_cast<DX12CommandList&>(command_list).GetDXRCommandList();
    auto* dx_command_signature = dynamic_cast<DX12CommandSignature&>(command_signature).GetCommandSignature();

    auto* dx_arguments_buffer = dynamic_cast<DX12Buffer&>(arguments_buffer).GetBuffer();
    
    dx_command_list->ExecuteIndirect(dx_command_signature, max_count, dx_arguments_buffer, arguments_buffer_offset, nullptr, 0 );
    
    return true; 
}

bool DX12Utils::ExecuteIndirect(IRHICommandList& command_list, IRHICommandSignature& command_signature,
                                unsigned max_count, IRHIBuffer& arguments_buffer, unsigned arguments_buffer_offset, IRHIBuffer& count_buffer,
                                unsigned count_buffer_offset)
{
    auto* dx_command_list = dynamic_cast<DX12CommandList&>(command_list).GetDXRCommandList();
    auto* dx_command_signature = dynamic_cast<DX12CommandSignature&>(command_signature).GetCommandSignature();

    auto* dx_arguments_buffer = dynamic_cast<DX12Buffer&>(arguments_buffer).GetBuffer();
    auto* dx_count_buffer = dynamic_cast<DX12Buffer&>(count_buffer).GetBuffer();
    
    dx_command_list->ExecuteIndirect(dx_command_signature, max_count, dx_arguments_buffer, arguments_buffer_offset, dx_count_buffer, count_buffer_offset );
    
    return true;
}

bool DX12Utils::Present(IRHISwapChain& swap_chain, IRHICommandQueue& command_queue, IRHICommandList& command_list)
{
    return swap_chain.Present(command_queue, command_list);
}

bool DX12Utils::DiscardResource(IRHICommandList& command_list, IRHIRenderTarget& render_target)
{
    auto* dx_command_list = dynamic_cast<DX12CommandList&>(command_list).GetCommandList();
    auto* dx_resource = dynamic_cast<DX12RenderTarget&>(render_target).GetResource();
    dx_command_list->DiscardResource(dx_resource, nullptr);

    return true;
}

bool DX12Utils::CopyTexture(IRHICommandList& commandList, IRHIRenderTarget& dst, IRHIRenderTarget& src)
{
    auto* dx_command_list = dynamic_cast<DX12CommandList&>(commandList).GetCommandList();
    
    D3D12_TEXTURE_COPY_LOCATION dstLocation;
    dstLocation.pResource = dynamic_cast<DX12RenderTarget&>(dst).GetResource();
    dstLocation.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX; 
    dstLocation.SubresourceIndex = 0;

    D3D12_TEXTURE_COPY_LOCATION srcLocation;
    srcLocation.pResource = dynamic_cast<DX12RenderTarget&>(src).GetResource();
    srcLocation.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX; 
    srcLocation.SubresourceIndex = 0;
    
    dx_command_list->CopyTextureRegion(&dstLocation, 0, 0, 0, &srcLocation, nullptr);
    
    return true;
}

bool DX12Utils::CopyBuffer(IRHICommandList& commandList, IRHIBuffer& dst, size_t dst_offset, IRHIBuffer& src,
    size_t src_offset, size_t size)
{
    auto* dx_command_list = dynamic_cast<DX12CommandList&>(commandList).GetCommandList();
    dx_command_list->CopyBufferRegion(dynamic_cast<DX12Buffer&>(dst).GetBuffer(), dst_offset, dynamic_cast<DX12Buffer&>(src).GetBuffer(), src_offset, size);
    
    return true;
}

bool DX12Utils::SupportRayTracing(IRHIDevice& device)
{
    auto* dxAdapter = dynamic_cast<DX12Device&>(device).GetAdapter();
    ComPtr<ID3D12Device5> testDevice;
    D3D12_FEATURE_DATA_D3D12_OPTIONS5 featureSupportData = {};

    return SUCCEEDED(D3D12CreateDevice(dxAdapter, D3D_FEATURE_LEVEL_11_0, IID_PPV_ARGS(&testDevice)))
        && SUCCEEDED(testDevice->CheckFeatureSupport(D3D12_FEATURE_D3D12_OPTIONS5, &featureSupportData, sizeof(featureSupportData)))
        && featureSupportData.RaytracingTier != D3D12_RAYTRACING_TIER_NOT_SUPPORTED;
}

unsigned DX12Utils::GetAlignmentSizeForUAVCount(unsigned size)
{
    const UINT alignment = D3D12_UAV_COUNTER_PLACEMENT_ALIGNMENT;
    return (size + (alignment - 1)) & ~(alignment - 1);
}
