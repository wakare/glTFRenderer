#include "DX12Utils.h"

#include <imgui/imgui.h>
#include <imgui/backends/imgui_impl_dx12.h>

#include <dxgidebug.h>
#include <d3d12shader.h>
#include <dxcapi.h>

#include "d3dx12.h"
#include "DX12CommandList.h"
#include "DX12CommandQueue.h"
#include "DX12CommandSignature.h"
#include "DX12ConverterUtils.h"
#include "DX12DescriptorHeap.h"
#include "DX12Device.h"
#include "DX12Fence.h"
#include "DX12Buffer.h"
#include "DX12DescriptorManager.h"
#include "DX12IndexBufferView.h"
#include "DX12PipelineStateObject.h"
#include "DX12RootSignature.h"
#include "DX12ShaderTable.h"
#include "DX12SwapChain.h"
#include "DX12Texture.h"
#include "DX12VertexBufferView.h"
#include "RHIInterface/IRHIFence.h"
#include "RHIInterface/IRHIBuffer.h"
#include "RHICommon.h"

// reference https://devblogs.microsoft.com/directx/gettingstarted-dx12agility/ SDK package is SDK 1.614.1 mapping 614 version
extern "C" { __declspec(dllexport) extern const UINT D3D12SDKVersion = 614; }
extern "C" { __declspec(dllexport) extern const char* D3D12SDKPath = ".\\D3D12\\"; }

bool DX12Utils::InitGraphicsAPI()
{
    return true;
}

bool DX12Utils::InitGUIContext(IRHIDevice& device, IRHICommandQueue& graphics_queue, IRHIDescriptorManager& descriptor_manager, unsigned back_buffer_count)
{
    auto* dx_device = dynamic_cast<DX12Device&>(device).GetDevice();
    auto& heap = dynamic_cast<DX12DescriptorManager&>(descriptor_manager).GetGUIDescriptorHeap();
    auto* dx_descriptor_heap = heap.GetDescriptorHeap();
    
    ImGui_ImplDX12_Init(dx_device, static_cast<int>(back_buffer_count),
        DXGI_FORMAT_R8G8B8A8_UNORM, dx_descriptor_heap,
        heap.GetAvailableCPUHandle(),
        heap.GetAvailableGPUHandle());
    
    return true;
}

bool DX12Utils::NewGUIFrame()
{
    ImGui_ImplDX12_NewFrame();

    return true;
}

bool DX12Utils::RenderGUIFrame(IRHICommandList& command_list)
{
    auto* dx_command_list = dynamic_cast<DX12CommandList&>(command_list).GetCommandList();
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

bool DX12Utils::BeginRendering(IRHICommandList& command_list, const RHIBeginRenderingInfo& begin_rendering_info)
{
    auto& render_targets = begin_rendering_info.m_render_targets;
    
    if (render_targets.empty())
    {
        return true;
    }
    
    auto* dxCommandList = dynamic_cast<DX12CommandList&>(command_list).GetCommandList();
    
    std::vector<D3D12_CPU_DESCRIPTOR_HANDLE> render_target_views;

    bool dsv_init = false;
    D3D12_CPU_DESCRIPTOR_HANDLE dsHandle{};
    
    for (size_t i = 0; i < render_targets.size(); ++i)
    {
        auto& render_target = *render_targets[i];
        auto handle = dynamic_cast<DX12TextureDescriptorAllocation&>(render_target).m_cpu_handle;
        if (render_target.GetDesc().m_view_type == RHIViewType::RVT_RTV)
        {
            render_target.m_source->Transition(command_list, RHIResourceStateType::STATE_RENDER_TARGET);
            render_target_views.push_back({handle});
        }
        else if (render_target.GetDesc().m_view_type == RHIViewType::RVT_DSV)
        {
            GLTF_CHECK(!dsv_init);
            dsv_init = true;
            render_target.m_source->Transition(command_list, begin_rendering_info.enable_depth_write ? RHIResourceStateType::STATE_DEPTH_WRITE : RHIResourceStateType::STATE_DEPTH_READ);
            dsHandle = {handle};    
        }
    }

    // TODO: Check RTsSingleHandleToDescriptorRange means?
    dxCommandList->OMSetRenderTargets(render_target_views.size(), render_target_views.data(), false, dsv_init ? &dsHandle : nullptr);

    D3D12_RECT render_area;
    render_area.left = begin_rendering_info.rendering_area_offset_x;
    render_area.top = begin_rendering_info.rendering_area_offset_y;
    render_area.right = begin_rendering_info.rendering_area_offset_x + begin_rendering_info.rendering_area_width;
    render_area.bottom = begin_rendering_info.rendering_area_offset_y + begin_rendering_info.rendering_area_height;
    
    for (size_t i = 0; i < render_targets.size(); ++i)
    {
        auto& render_target = dynamic_cast<const IRHITextureDescriptorAllocation&>(*render_targets[i]);
        auto clear_value = render_target.m_source->GetTextureDesc().GetClearValue();
        
        auto dx_render_target_clear_value = DX12ConverterUtils::ConvertToD3DClearValue(clear_value);
        auto dx_depth_stencil_clear_value = DX12ConverterUtils::ConvertToD3DClearValue(clear_value);
        
        auto handle = dynamic_cast<const DX12TextureDescriptorAllocation&>(render_target).m_cpu_handle;
        switch (render_target.GetDesc().m_view_type)
        {
        case RHIViewType::RVT_RTV:
            {
                if (begin_rendering_info.clear_render_target)
                {
                    dxCommandList->ClearRenderTargetView({handle}, dx_render_target_clear_value.Color, 1, &render_area);    
                }   
            }
            break;

        case RHIViewType::RVT_DSV:
            {
                if (begin_rendering_info.enable_depth_write && begin_rendering_info.clear_depth_stencil)
                {
                    dxCommandList->ClearDepthStencilView({handle}, D3D12_CLEAR_FLAG_DEPTH | D3D12_CLEAR_FLAG_STENCIL,
                        dx_render_target_clear_value.DepthStencil.Depth, dx_depth_stencil_clear_value.DepthStencil.Stencil, 1, &render_area);
                }
            }
            break;
            
        default:
            GLTF_CHECK(false);
        }
    }
    
    return true;
}

bool DX12Utils::EndRendering(IRHICommandList& command_list)
{
    return true;
}

bool DX12Utils::ResetCommandList(IRHICommandList& command_list, IRHICommandAllocator& command_allocator,
                                 IRHIPipelineStateObject* init_pso)
{
    auto* dx_command_list = dynamic_cast<DX12CommandList&>(command_list).GetCommandList();
    auto* dx_command_allocator = dynamic_cast<DX12CommandAllocator&>(command_allocator).GetCommandAllocator();
    if (init_pso)
    {
        const auto& dxPSO = dynamic_cast<IDX12PipelineStateObjectCommon&>(*init_pso);
        
        if (init_pso->GetPSOType() == RHIPipelineType::RayTracing)
        {
            auto* dxr_command_list = dynamic_cast<DX12CommandList&>(command_list).GetDXRCommandList();
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

bool DX12Utils::CloseCommandList(IRHICommandList& command_list)
{
    auto* dxCommandList = dynamic_cast<DX12CommandList&>(command_list).GetCommandList();
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

bool DX12Utils::ResetCommandAllocator(IRHICommandAllocator& command_allocator)
{
    auto* dxCommandAllocator = dynamic_cast<DX12CommandAllocator&>(command_allocator).GetCommandAllocator();
    THROW_IF_FAILED(dxCommandAllocator->Reset())
    return true;
}

bool DX12Utils::WaitCommandListFinish(IRHICommandList& command_list)
{
    return command_list.WaitCommandList();
}

bool DX12Utils::WaitCommandQueueIdle(IRHICommandQueue& command_queue)
{
    return true;
}

bool DX12Utils::WaitDeviceIdle(IRHIDevice& device)
{
    return true;
}

bool DX12Utils::SetRootSignature(IRHICommandList& command_list, IRHIRootSignature& rootSignature,IRHIPipelineStateObject& pipeline_state_object, RHIPipelineType pipeline_type)
{
    auto* dxCommandList = dynamic_cast<DX12CommandList&>(command_list).GetCommandList();
    auto* dxRootSignature = dynamic_cast<DX12RootSignature&>(rootSignature).GetRootSignature();

    if (pipeline_type == RHIPipelineType::Graphics)
    {
        dxCommandList->SetGraphicsRootSignature(dxRootSignature);    
    }
    else
    {
        dxCommandList->SetComputeRootSignature(dxRootSignature);
    }
    
    return true;
}

bool DX12Utils::SetViewport(IRHICommandList& command_list, const RHIViewportDesc& viewport_desc)
{
    auto* dxCommandList = dynamic_cast<DX12CommandList&>(command_list).GetCommandList();

    D3D12_VIEWPORT viewport = {viewport_desc.top_left_x, viewport_desc.top_left_y, viewport_desc.width, viewport_desc.height, viewport_desc.min_depth, viewport_desc.max_depth};
    dxCommandList->RSSetViewports(1, &viewport);
    
    return true;
}

bool DX12Utils::SetScissorRect(IRHICommandList& command_list, const RHIScissorRectDesc& scissor_rect)
{
    auto* dxCommandList = dynamic_cast<DX12CommandList&>( command_list).GetCommandList();
    D3D12_RECT dxScissorRect =
        {
            static_cast<LONG>(scissor_rect.left), static_cast<LONG>(scissor_rect.top),
            static_cast<LONG>(scissor_rect.right), static_cast<LONG>(scissor_rect.bottom)
        };
    
    dxCommandList->RSSetScissorRects(1, &dxScissorRect);
    
    return true;
}

bool DX12Utils::SetVertexBufferView(IRHICommandList& command_list, unsigned slot, IRHIVertexBufferView& view)
{
    auto* dxCommandList = dynamic_cast<DX12CommandList&>(command_list).GetCommandList();
    auto& dxVertexBufferView = dynamic_cast<DX12VertexBufferView&>(view).GetVertexBufferView();

    dxCommandList->IASetVertexBuffers(slot, 1, &dxVertexBufferView);
    
    return true;
}

bool DX12Utils::SetIndexBufferView(IRHICommandList& command_list, IRHIIndexBufferView& view)
{
    auto* dxCommandList = dynamic_cast<DX12CommandList&>(command_list).GetCommandList();
    auto& dxIndexBufferView = dynamic_cast<DX12IndexBufferView&>(view).GetIndexBufferView();

    dxCommandList->IASetIndexBuffer(&dxIndexBufferView);
    
    return true;
}

bool DX12Utils::SetPrimitiveTopology(IRHICommandList& command_list, RHIPrimitiveTopologyType type)
{
    auto* dxCommandList = dynamic_cast<DX12CommandList&>(command_list).GetCommandList();
    dxCommandList->IASetPrimitiveTopology(DX12ConverterUtils::ConvertToPrimitiveTopologyType(type));
    
    return true;
}

bool DX12Utils::SetDescriptorHeapArray(IRHICommandList& command_list, DX12DescriptorHeap* descriptor_heap_array_data,
                                  size_t descriptor_heap_array_count)
{
    auto* dxCommandList = dynamic_cast<DX12CommandList&>(command_list).GetCommandList();

    std::vector<ID3D12DescriptorHeap*> dx_descriptor_heaps;
    for (size_t i = 0; i < descriptor_heap_array_count; ++i)
    {
        auto* dx_descriptor_heap = descriptor_heap_array_data[i].GetDescriptorHeap();
        dx_descriptor_heaps.push_back(dx_descriptor_heap);
    }
    
    dxCommandList->SetDescriptorHeaps(dx_descriptor_heaps.size(), dx_descriptor_heaps.data());
    
    return true;
}

bool DX12Utils::SetConstant32BitToRootParameterSlot(IRHICommandList& command_list, unsigned slot_index, unsigned* data,
                                                    unsigned count, RHIPipelineType pipeline_type)
{
    auto* dxCommandList = dynamic_cast<DX12CommandList&>(command_list).GetCommandList();
    if (pipeline_type == RHIPipelineType::Graphics)
    {
        for (unsigned i = 0; i < count; ++i)
        {
            dxCommandList->SetGraphicsRoot32BitConstant(slot_index, data[i], i);    
        }
            
    }
    else
    {
        for (unsigned i = 0; i < count; ++i)
        {
            dxCommandList->SetComputeRoot32BitConstant(slot_index, data[i], i);    
        }
    }
    
    return true;
}

bool DX12Utils::SetCBVToRootParameterSlot(IRHICommandList& command_list, unsigned slot_index,
                                          const IRHIDescriptorAllocation& handle, bool isGraphicsPipeline)
{
    auto* dxCommandList = dynamic_cast<DX12CommandList&>(command_list).GetCommandList();
    auto gpu_handle = dynamic_cast<const DX12BufferDescriptorAllocation&>(handle).m_gpu_handle;
    
    if (isGraphicsPipeline)
    {
        dxCommandList->SetGraphicsRootConstantBufferView(slot_index, gpu_handle);    
    }
    else
    {
        dxCommandList->SetComputeRootConstantBufferView(slot_index, gpu_handle);
    }
    
    return true;
}

bool DX12Utils::SetSRVToRootParameterSlot(IRHICommandList& command_list, unsigned slot_index,
                                          const IRHIDescriptorAllocation& handle, bool isGraphicsPipeline)
{
    auto* dxCommandList = dynamic_cast<DX12CommandList&>(command_list).GetCommandList();
    auto gpu_handle = dynamic_cast<const DX12BufferDescriptorAllocation&>(handle).m_gpu_handle;

    if (isGraphicsPipeline)
    {
        dxCommandList->SetGraphicsRootShaderResourceView(slot_index, gpu_handle);    
    }
    else
    {
        dxCommandList->SetComputeRootShaderResourceView(slot_index, gpu_handle);
    }
    
    return true;
}

bool DX12Utils::SetUAVToRootParameterSlot(IRHICommandList& command_list, unsigned slot_index,
    const IRHIDescriptorAllocation& handle, bool isGraphicsPipeline)
{
    auto* dxCommandList = dynamic_cast<DX12CommandList&>(command_list).GetCommandList();
    auto gpu_handle = dynamic_cast<const DX12BufferDescriptorAllocation&>(handle).m_gpu_handle;

    if (isGraphicsPipeline)
    {
        dxCommandList->SetGraphicsRootUnorderedAccessView(slot_index, gpu_handle);    
    }
    else
    {
        dxCommandList->SetComputeRootUnorderedAccessView(slot_index, gpu_handle);
    }
    
    return true;
}

bool DX12Utils::SetDTToRootParameterSlot(IRHICommandList& command_list, unsigned slot_index,
                                         const IRHIDescriptorAllocation& handle, bool isGraphicsPipeline)
{
    auto* dxCommandList = dynamic_cast<DX12CommandList&>(command_list).GetCommandList();
    
    D3D12_GPU_DESCRIPTOR_HANDLE dxHandle{};
    if (handle.GetDesc().IsBufferDescriptor())
    {
        dxHandle = {dynamic_cast<const DX12BufferDescriptorAllocation&>(handle).m_gpu_handle}; 
    }
    else
    {
        dxHandle = {dynamic_cast<const DX12TextureDescriptorAllocation&>(handle).m_gpu_handle};
    }
    
    if (isGraphicsPipeline)
    {
        dxCommandList->SetGraphicsRootDescriptorTable(slot_index, dxHandle);    
    }
    else
    {
        dxCommandList->SetComputeRootDescriptorTable(slot_index, dxHandle);
    }
    
    return true;
}

bool DX12Utils::SetDTToRootParameterSlot(IRHICommandList& command_list, unsigned slot_index,
    const IRHIDescriptorTable& table_handle, bool isGraphicsPipeline)
{
    auto* dxCommandList = dynamic_cast<DX12CommandList&>(command_list).GetCommandList();
    D3D12_GPU_DESCRIPTOR_HANDLE dxHandle = {dynamic_cast<const DX12DescriptorTable&>(table_handle).m_gpu_handle};
    
    if (isGraphicsPipeline)
    {
        dxCommandList->SetGraphicsRootDescriptorTable(slot_index, dxHandle);    
    }
    else
    {
        dxCommandList->SetComputeRootDescriptorTable(slot_index, dxHandle);
    }
    
    return true;
}

bool DX12Utils::UploadBufferDataToDefaultGPUBuffer(IRHICommandList& command_list, IRHIBuffer& upload_buffer,
                                                   IRHIBuffer& default_buffer, void* data, size_t size)
{
    auto* dxCommandList = dynamic_cast<DX12CommandList&>(command_list).GetCommandList();
    auto* dxUploadBuffer = dynamic_cast<DX12Buffer&>(upload_buffer).GetRawBuffer();
    auto* dxDefaultBuffer = dynamic_cast<DX12Buffer&>(default_buffer).GetRawBuffer();
    
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

bool DX12Utils::UploadTextureDataToDefaultGPUBuffer(IRHICommandList& command_list, IRHIBuffer& upload_buffer,
    IRHIBuffer& default_buffer, void* data, size_t row_pitch, size_t slice_pitch)
{
    auto* dx_command_list = dynamic_cast<DX12CommandList&>(command_list).GetCommandList();
    auto* dx_upload_buffer = dynamic_cast<DX12Buffer&>(upload_buffer).GetRawBuffer();
    auto* dx_default_buffer = dynamic_cast<DX12Buffer&>(default_buffer).GetRawBuffer();
    
    D3D12_SUBRESOURCE_DATA upload_data = {};
    upload_data.pData = static_cast<BYTE*>(data);
    upload_data.RowPitch = row_pitch;
    upload_data.SlicePitch = slice_pitch;

    UpdateSubresources(dx_command_list, dx_default_buffer, dx_upload_buffer, 0, 0, 1, &upload_data);

    return true;
}

bool DX12Utils::AddBufferBarrierToCommandList(IRHICommandList& command_list, const IRHIBuffer& buffer,
                                              RHIResourceStateType beforeState, RHIResourceStateType afterState)
{
    auto* dxCommandList = dynamic_cast<DX12CommandList&>(command_list).GetCommandList();
    auto* dxBuffer = dynamic_cast<const DX12Buffer&>(buffer).GetRawBuffer();
    
    CD3DX12_RESOURCE_BARRIER TransitionToVertexBufferState = CD3DX12_RESOURCE_BARRIER::Transition(dxBuffer,
        DX12ConverterUtils::ConvertToResourceState(beforeState), DX12ConverterUtils::ConvertToResourceState(afterState)); 
    dxCommandList->ResourceBarrier(1, &TransitionToVertexBufferState);

    return true;
}

bool DX12Utils::AddTextureBarrierToCommandList(IRHICommandList& command_list, IRHITexture& buffer,
                                               RHIResourceStateType beforeState, RHIResourceStateType afterState)
{
    auto* dxCommandList = dynamic_cast<DX12CommandList&>(command_list).GetCommandList();
    auto* dxBuffer = dynamic_cast<DX12Texture&>(buffer).GetRawResource();
    
    CD3DX12_RESOURCE_BARRIER TransitionToVertexBufferState = CD3DX12_RESOURCE_BARRIER::Transition(dxBuffer,
        DX12ConverterUtils::ConvertToResourceState(beforeState), DX12ConverterUtils::ConvertToResourceState(afterState)); 
    dxCommandList->ResourceBarrier(1, &TransitionToVertexBufferState);

    return true;
}

bool DX12Utils::AddUAVBarrier(IRHICommandList& command_list, IRHITexture& texture)
{
    auto* dxCommandList = dynamic_cast<DX12CommandList&>(command_list).GetCommandList();
    auto* dxBuffer = dynamic_cast<DX12Texture&>(texture).GetRawResource();

    CD3DX12_RESOURCE_BARRIER UAV_barrier = CD3DX12_RESOURCE_BARRIER::UAV(dxBuffer); 
    dxCommandList->ResourceBarrier(1, &UAV_barrier);

    return true;
}

bool DX12Utils::DrawInstanced(IRHICommandList& command_list, unsigned vertex_count_per_instance, unsigned instance_count,
                              unsigned start_vertex_location, unsigned start_instance_location)
{
    auto* dxCommandList = dynamic_cast<DX12CommandList&>(command_list).GetCommandList();
    dxCommandList->DrawInstanced(vertex_count_per_instance, instance_count, start_vertex_location, start_instance_location);
    
    return true;
}

bool DX12Utils::DrawIndexInstanced(IRHICommandList& command_list, unsigned index_count_per_instance, unsigned instance_count,
                                   unsigned start_index_location, unsigned base_vertex_location, unsigned start_instance_location)
{
    auto* dxCommandList = dynamic_cast<DX12CommandList&>(command_list).GetCommandList();
    dxCommandList->DrawIndexedInstanced(index_count_per_instance, instance_count, start_index_location, static_cast<INT>(base_vertex_location), start_instance_location);
    
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
    unsigned max_count, IRHIBuffer& arguments_buffer, unsigned arguments_buffer_offset,  unsigned command_stride)
{
    auto* dx_command_list = dynamic_cast<DX12CommandList&>(command_list).GetDXRCommandList();
    auto* dx_command_signature = dynamic_cast<DX12CommandSignature&>(command_signature).GetCommandSignature();

    auto* dx_arguments_buffer = dynamic_cast<DX12Buffer&>(arguments_buffer).GetRawBuffer();
    
    dx_command_list->ExecuteIndirect(dx_command_signature, max_count, dx_arguments_buffer, arguments_buffer_offset, nullptr, 0 );
    
    return true; 
}

bool DX12Utils::ExecuteIndirect(IRHICommandList& command_list, IRHICommandSignature& command_signature,
                                unsigned max_count, IRHIBuffer& arguments_buffer, unsigned arguments_buffer_offset, IRHIBuffer& count_buffer,
                                unsigned count_buffer_offset, unsigned command_stride)
{
    auto* dx_command_list = dynamic_cast<DX12CommandList&>(command_list).GetDXRCommandList();
    auto* dx_command_signature = dynamic_cast<DX12CommandSignature&>(command_signature).GetCommandSignature();

    auto* dx_arguments_buffer = dynamic_cast<DX12Buffer&>(arguments_buffer).GetRawBuffer();
    auto* dx_count_buffer = dynamic_cast<DX12Buffer&>(count_buffer).GetRawBuffer();
    
    dx_command_list->ExecuteIndirect(dx_command_signature, max_count, dx_arguments_buffer, arguments_buffer_offset, dx_count_buffer, count_buffer_offset );
    
    return true;
}

bool DX12Utils::CopyTexture(IRHICommandList& command_list, IRHITexture& dst, IRHITexture& src, const RHICopyTextureInfo& copy_info)
{
    auto* dx_command_list = dynamic_cast<DX12CommandList&>(command_list).GetCommandList();
    dst.Transition(command_list, RHIResourceStateType::STATE_COPY_DEST);
    src.Transition(command_list, RHIResourceStateType::STATE_COPY_SOURCE);
    
    D3D12_TEXTURE_COPY_LOCATION dstLocation;
    dstLocation.pResource = dynamic_cast<DX12Texture&>(dst).GetRawResource();
    dstLocation.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX; 
    dstLocation.SubresourceIndex = copy_info.dst_mip_level;

    D3D12_TEXTURE_COPY_LOCATION srcLocation;
    srcLocation.pResource = dynamic_cast<DX12Texture&>(src).GetRawResource();
    srcLocation.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX; 
    srcLocation.SubresourceIndex = copy_info.src_mip_level;
    
    auto copy_width = copy_info.copy_width ? copy_info.copy_width : dst.GetTextureDesc().GetTextureWidth();
    auto copy_height = copy_info.copy_height ? copy_info.copy_height : dst.GetTextureDesc().GetTextureHeight();
    
    D3D12_BOX src_box{};
    src_box.left = 0;
    src_box.right = copy_width;
    src_box.top = 0;
    src_box.bottom = copy_height;
    src_box.front = 0;
    src_box.back = 1;
    
    dx_command_list->CopyTextureRegion(&dstLocation, copy_info.dst_x, copy_info.dst_y, 0, &srcLocation, &src_box);
    
    return true;
}

bool DX12Utils::CopyTexture(IRHICommandList& command_list, IRHITexture& dst, IRHIBuffer& src,
    const RHICopyTextureInfo& copy_info)
{
    auto* dx_command_list = dynamic_cast<DX12CommandList&>(command_list).GetCommandList();
    dst.Transition(command_list, RHIResourceStateType::STATE_COPY_DEST);
    src.Transition(command_list, RHIResourceStateType::STATE_COPY_SOURCE);
    
    auto copy_width = copy_info.copy_width ? copy_info.copy_width : dst.GetTextureDesc().GetTextureWidth();
    auto copy_height = copy_info.copy_height ? copy_info.copy_height : dst.GetTextureDesc().GetTextureHeight();
    
    D3D12_TEXTURE_COPY_LOCATION dstLocation;
    dstLocation.pResource = dynamic_cast<DX12Texture&>(dst).GetRawResource();
    dstLocation.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX; 
    dstLocation.SubresourceIndex = copy_info.dst_mip_level;

    D3D12_TEXTURE_COPY_LOCATION src_location = {};
    src_location.pResource = dynamic_cast<DX12Buffer&>(src).GetRawBuffer();
    src_location.Type = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;

    D3D12_PLACED_SUBRESOURCE_FOOTPRINT footprint = {};
    footprint.Offset = 0;
    footprint.Footprint.Width = copy_width;
    footprint.Footprint.Height = copy_height;
    footprint.Footprint.Depth = 1;
    footprint.Footprint.RowPitch = copy_info.row_pitch;
    footprint.Footprint.Format = DX12ConverterUtils::ConvertToDXGIFormat(dst.GetTextureFormat());

    src_location.PlacedFootprint = footprint;
    
    D3D12_BOX src_box{};
    src_box.left = 0;
    src_box.right = copy_width;
    src_box.top = 0;
    src_box.bottom = copy_height;
    src_box.front = 0;
    src_box.back = 1;
    
    dx_command_list->CopyTextureRegion(&dstLocation, copy_info.dst_x, copy_info.dst_y, 0, &src_location, &src_box);
    
    return true;
}

bool DX12Utils::CopyBuffer(IRHICommandList& command_list, IRHIBuffer& dst, size_t dst_offset, IRHIBuffer& src,
                           size_t src_offset, size_t size)
{
    auto* dx_command_list = dynamic_cast<DX12CommandList&>(command_list).GetCommandList();
    
    dst.Transition(command_list, RHIResourceStateType::STATE_COPY_DEST);
    src.Transition(command_list, RHIResourceStateType::STATE_COPY_SOURCE);
    dx_command_list->CopyBufferRegion(dynamic_cast<DX12Buffer&>(dst).GetRawBuffer(), dst_offset, dynamic_cast<DX12Buffer&>(src).GetRawBuffer(), src_offset, size);
    
    return true;
}

bool DX12Utils::ClearUAVTexture(IRHICommandList& command_list, const IRHITextureDescriptorAllocation& texture_descriptor)
{
    auto* dx_command_list = dynamic_cast<DX12CommandList&>(command_list).GetCommandList();
    const auto& dx_texture_descriptor = dynamic_cast<const DX12TextureDescriptorAllocation&>(texture_descriptor);
    auto* dx_texture_resource = dynamic_cast<DX12Texture&>(*dx_texture_descriptor.m_source).GetRawResource();
    
    GLTF_CHECK(texture_descriptor.m_view_desc->m_view_type == RHIViewType::RVT_UAV);
    if (IsFloatDataFormat(texture_descriptor.m_view_desc->m_format))
    {
        static float clear_value[4] = {0.0f, 0.0f, 0.0f, 0.0f};
        dx_command_list->ClearUnorderedAccessViewFloat(D3D12_GPU_DESCRIPTOR_HANDLE(dx_texture_descriptor.m_gpu_handle),
            D3D12_CPU_DESCRIPTOR_HANDLE(dx_texture_descriptor.m_cpu_handle),  dx_texture_resource, clear_value, 0, nullptr);
    }
    else
    {
        static UINT clear_value[4] = {0, 0, 0, 0};
        dx_command_list->ClearUnorderedAccessViewUint(D3D12_GPU_DESCRIPTOR_HANDLE(dx_texture_descriptor.m_gpu_handle),
            D3D12_CPU_DESCRIPTOR_HANDLE(dx_texture_descriptor.m_cpu_handle),  dx_texture_resource, clear_value, 0, nullptr);
    }
    
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

void DX12Utils::ReportLiveObjects()
{
    LOG_FLUSH("[DX12] Report Live Objects---------------------------------\n");
    ComPtr<IDXGIDebug1> dxgi_debug;
    if (SUCCEEDED(DXGIGetDebugInterface1(0, IID_PPV_ARGS(dxgi_debug.GetAddressOf()))))
    {
        dxgi_debug->ReportLiveObjects(DXGI_DEBUG_ALL, DXGI_DEBUG_RLO_FLAGS(DXGI_DEBUG_RLO_DETAIL | DXGI_DEBUG_RLO_IGNORE_INTERNAL));
    }
}

bool DX12Utils::RegisterShaderParameterToRootSignature(const IRHIShader& shader,
    IRHIRootSignatureHelper& root_signature_helper)
{
    ComPtr<IDxcUtils> utils; DxcCreateInstance(CLSID_DxcUtils, IID_PPV_ARGS(&utils));
    ComPtr<ID3D12ShaderReflection> refl;

    const auto& shader_bytecode = shader.GetShaderByteCode();
    DxcBuffer shader_bytecode_buffer{};
    shader_bytecode_buffer.Ptr = shader_bytecode.data();
    shader_bytecode_buffer.Size = shader_bytecode.size();
    utils->CreateReflection(&shader_bytecode_buffer, IID_PPV_ARGS(&refl));

    D3D12_SHADER_DESC sd{};
    refl->GetDesc(&sd);

    for (UINT i = 0; i < sd.BoundResources; ++i)
    {
        D3D12_SHADER_INPUT_BIND_DESC bd{};
        refl->GetResourceBindingDesc(i, &bd);

        LOG_FORMAT_FLUSH("[Reflect] Shader %s contains var name:%s\nbinding: %d\nset index:%d\n", shader.GetMainEntry().c_str(), bd.Name,
            bd.BindPoint, bd.Space);
    }
    
    return true;
}

DX12Utils& DX12Utils::DX12Instance()
{
    return static_cast<DX12Utils&>(RHIUtilInstanceManager::Instance());
}
