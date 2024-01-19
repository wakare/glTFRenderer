#include "DX12Utils.h"

#include <imgui/imgui.h>
#include <imgui/backends/imgui_impl_dx12.h>

#include "d3dx12.h"
#include "DX12CommandList.h"
#include "DX12CommandQueue.h"
#include "DX12CommandSignature.h"
#include "DX12DescriptorHeap.h"
#include "DX12Device.h"
#include "DX12GPUBuffer.h"
#include "DX12IndexBufferView.h"
#include "DX12PipelineStateObject.h"
#include "DX12RenderTarget.h"
#include "DX12RootSignature.h"
#include "DX12ShaderTable.h"
#include "DX12SwapChain.h"
#include "DX12VertexBufferView.h"
#include "glTFRHI/RHIInterface/IRHIGPUBuffer.h"
#include "glTFRHI/RHIInterface/RHICommon.h"

#define CONVERT_DXGI_FORMAT_CASE(RHIFormat) case RHIDataFormat::##RHIFormat: return DXGI_FORMAT_##RHIFormat;

DXGI_FORMAT DX12ConverterUtils::ConvertToDXGIFormat(RHIDataFormat format)
{
    switch (format)
    {
        CONVERT_DXGI_FORMAT_CASE(R8G8B8A8_UNORM)
        CONVERT_DXGI_FORMAT_CASE(R8G8B8A8_UNORM_SRGB)
        CONVERT_DXGI_FORMAT_CASE(B8G8R8A8_UNORM)
        CONVERT_DXGI_FORMAT_CASE(B8G8R8A8_UNORM_SRGB)
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
        CONVERT_DXGI_FORMAT_CASE(R32G32B32A32_UINT)
        CONVERT_DXGI_FORMAT_CASE(R32_UINT)
        CONVERT_DXGI_FORMAT_CASE(R32_TYPELESS)
        CONVERT_DXGI_FORMAT_CASE(R16_FLOAT)
        CONVERT_DXGI_FORMAT_CASE(R16_UNORM)
        CONVERT_DXGI_FORMAT_CASE(R16_UINT)
        CONVERT_DXGI_FORMAT_CASE(R8_UNORM)
        CONVERT_DXGI_FORMAT_CASE(A8_UNORM)    
        case RHIDataFormat::Unknown: return DXGI_FORMAT_UNKNOWN;
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
    case RHIResourceStateType::STATE_COMMON:
        return D3D12_RESOURCE_STATE_COMMON;

    case RHIResourceStateType::STATE_GENERIC_READ:
        return D3D12_RESOURCE_STATE_GENERIC_READ;
        
    case RHIResourceStateType::STATE_COPY_SOURCE:
        return D3D12_RESOURCE_STATE_COPY_SOURCE;
        
    case RHIResourceStateType::STATE_COPY_DEST:
        return D3D12_RESOURCE_STATE_COPY_DEST;
        
    case RHIResourceStateType::STATE_VERTEX_AND_CONSTANT_BUFFER:
        return D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER;

    case RHIResourceStateType::STATE_INDEX_BUFFER:
        return D3D12_RESOURCE_STATE_INDEX_BUFFER;

    case RHIResourceStateType::STATE_PRESENT:
        return D3D12_RESOURCE_STATE_PRESENT;

    case RHIResourceStateType::STATE_RENDER_TARGET:
        return D3D12_RESOURCE_STATE_RENDER_TARGET;

    case RHIResourceStateType::STATE_DEPTH_WRITE:
        return D3D12_RESOURCE_STATE_DEPTH_WRITE;

    case RHIResourceStateType::STATE_DEPTH_READ:
        return D3D12_RESOURCE_STATE_DEPTH_READ;

    case RHIResourceStateType::STATE_UNORDERED_ACCESS:
        return D3D12_RESOURCE_STATE_UNORDERED_ACCESS;
        
    case RHIResourceStateType::STATE_NON_PIXEL_SHADER_RESOURCE:
        return D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE;
        
    case RHIResourceStateType::STATE_PIXEL_SHADER_RESOURCE:
        return D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;

    case RHIResourceStateType::STATE_RAYTRACING_ACCELERATION_STRUCTURE:
        return D3D12_RESOURCE_STATE_RAYTRACING_ACCELERATION_STRUCTURE;
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

#define COVERT_TO_SRV_DIMENSION_CASE(dimensionType) case RHIResourceDimension::##dimensionType: return D3D12_SRV_DIMENSION_##dimensionType;

D3D12_SRV_DIMENSION DX12ConverterUtils::ConvertToSRVDimensionType(RHIResourceDimension type)
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

#define COVERT_TO_UAV_DIMENSION_CASE(dimensionType) case RHIResourceDimension::##dimensionType: return D3D12_UAV_DIMENSION_##dimensionType;

D3D12_UAV_DIMENSION DX12ConverterUtils::ConvertToUAVDimensionType(RHIResourceDimension type)
{
    switch (type) {
        COVERT_TO_UAV_DIMENSION_CASE(BUFFER)
        COVERT_TO_UAV_DIMENSION_CASE(TEXTURE1D)
        COVERT_TO_UAV_DIMENSION_CASE(TEXTURE1DARRAY)
        COVERT_TO_UAV_DIMENSION_CASE(TEXTURE2D)
        COVERT_TO_UAV_DIMENSION_CASE(TEXTURE2DARRAY)
        COVERT_TO_UAV_DIMENSION_CASE(TEXTURE3D)
    }

    return D3D12_UAV_DIMENSION_UNKNOWN;
}

D3D12_CLEAR_VALUE DX12ConverterUtils::ConvertToD3DClearValue(RHIRenderTargetClearValue clear_value)
{
    D3D12_CLEAR_VALUE dx_clear_value = {};
    dx_clear_value.Format = ConvertToDXGIFormat(clear_value.clear_format);
    memcpy(dx_clear_value.Color, &clear_value.clear_color, sizeof(clear_value.clear_color));
    dx_clear_value.DepthStencil.Depth = clear_value.clearDS.clear_depth;
    dx_clear_value.DepthStencil.Stencil = clear_value.clearDS.clear_stencil_value;

    return dx_clear_value;
}

D3D12_INDIRECT_ARGUMENT_TYPE DX12ConverterUtils::ConvertToIndirectArgumentType(RHIIndirectArgType type)
{
    switch (type) {
    case RHIIndirectArgType::Draw:
        return D3D12_INDIRECT_ARGUMENT_TYPE_DRAW;
        
    case RHIIndirectArgType::DrawIndexed:
        return D3D12_INDIRECT_ARGUMENT_TYPE_DRAW_INDEXED;
        
    case RHIIndirectArgType::Dispatch:
        return D3D12_INDIRECT_ARGUMENT_TYPE_DISPATCH;
        
    case RHIIndirectArgType::VertexBufferView:
        return D3D12_INDIRECT_ARGUMENT_TYPE_VERTEX_BUFFER_VIEW;
        
    case RHIIndirectArgType::IndexBufferView:
        return D3D12_INDIRECT_ARGUMENT_TYPE_INDEX_BUFFER_VIEW;
        
    case RHIIndirectArgType::Constant:
        return D3D12_INDIRECT_ARGUMENT_TYPE_CONSTANT;
        
    case RHIIndirectArgType::ConstantBufferView:
        return D3D12_INDIRECT_ARGUMENT_TYPE_CONSTANT_BUFFER_VIEW;
        
    case RHIIndirectArgType::ShaderResourceView:
        return D3D12_INDIRECT_ARGUMENT_TYPE_SHADER_RESOURCE_VIEW;
        
    case RHIIndirectArgType::UnOrderAccessView:
        return D3D12_INDIRECT_ARGUMENT_TYPE_UNORDERED_ACCESS_VIEW;
        
    case RHIIndirectArgType::DispatchRays:
        return D3D12_INDIRECT_ARGUMENT_TYPE_DISPATCH_RAYS;
        
    case RHIIndirectArgType::DispatchMesh:
        return D3D12_INDIRECT_ARGUMENT_TYPE_DISPATCH_MESH;
    }

    GLTF_CHECK(false);
    return D3D12_INDIRECT_ARGUMENT_TYPE_DISPATCH;
}

D3D12_INDIRECT_ARGUMENT_DESC DX12ConverterUtils::ConvertToIndirectArgumentDesc(const RHIIndirectArgumentDesc& desc)
{
    D3D12_INDIRECT_ARGUMENT_DESC result = {};

    result.Type = ConvertToIndirectArgumentType(desc.type);
    switch (result.Type) {
    case D3D12_INDIRECT_ARGUMENT_TYPE_DRAW:
        break;
    case D3D12_INDIRECT_ARGUMENT_TYPE_DRAW_INDEXED:
        break;
    case D3D12_INDIRECT_ARGUMENT_TYPE_DISPATCH:
        break;
        
    case D3D12_INDIRECT_ARGUMENT_TYPE_VERTEX_BUFFER_VIEW:
        result.VertexBuffer.Slot = desc.desc.vertex_buffer.slot;
        break;
    case D3D12_INDIRECT_ARGUMENT_TYPE_INDEX_BUFFER_VIEW:
        break;
        
    case D3D12_INDIRECT_ARGUMENT_TYPE_CONSTANT:
        result.Constant.RootParameterIndex = desc.desc.constant.root_parameter_index;
        result.Constant.DestOffsetIn32BitValues = desc.desc.constant.dest_offset_in_32bit_value;
        result.Constant.Num32BitValuesToSet = desc.desc.constant.num_32bit_values_to_set;
        break;
        
    case D3D12_INDIRECT_ARGUMENT_TYPE_CONSTANT_BUFFER_VIEW:
        result.ConstantBufferView.RootParameterIndex = desc.desc.constant_buffer_view.root_parameter_index;
        break;
        
    case D3D12_INDIRECT_ARGUMENT_TYPE_SHADER_RESOURCE_VIEW:
        result.ShaderResourceView.RootParameterIndex = desc.desc.shader_resource_view.root_parameter_index;
        break;
        
    case D3D12_INDIRECT_ARGUMENT_TYPE_UNORDERED_ACCESS_VIEW:
        result.UnorderedAccessView.RootParameterIndex = desc.desc.unordered_access_view.root_parameter_index;
        break;
        
    case D3D12_INDIRECT_ARGUMENT_TYPE_DISPATCH_RAYS:
        break;
    case D3D12_INDIRECT_ARGUMENT_TYPE_DISPATCH_MESH:
        break;
    }
    
    return result;
}


DX12Utils::~DX12Utils()
{
}

int DX12Utils::GetDXGIFormatBitsPerPixel(const DXGI_FORMAT& dxgiFormat)
{
    if (dxgiFormat == DXGI_FORMAT_R32G32B32A32_FLOAT) return 128;
    else if (dxgiFormat == DXGI_FORMAT_R16G16B16A16_FLOAT) return 64;
    else if (dxgiFormat == DXGI_FORMAT_R16G16B16A16_UNORM) return 64;
    else if (dxgiFormat == DXGI_FORMAT_R8G8B8A8_UNORM) return 32;
    else if (dxgiFormat == DXGI_FORMAT_R8G8B8A8_UNORM_SRGB) return 32;
    else if (dxgiFormat == DXGI_FORMAT_B8G8R8A8_UNORM) return 32;
    else if (dxgiFormat == DXGI_FORMAT_B8G8R8A8_UNORM_SRGB) return 32;
    else if (dxgiFormat == DXGI_FORMAT_B8G8R8X8_UNORM) return 32;
    else if (dxgiFormat == DXGI_FORMAT_R10G10B10_XR_BIAS_A2_UNORM) return 32;

    else if (dxgiFormat == DXGI_FORMAT_R10G10B10A2_UNORM) return 32;
    else if (dxgiFormat == DXGI_FORMAT_B5G5R5A1_UNORM) return 16;
    else if (dxgiFormat == DXGI_FORMAT_B5G6R5_UNORM) return 16;
    else if (dxgiFormat == DXGI_FORMAT_R32_FLOAT) return 32;
    else if (dxgiFormat == DXGI_FORMAT_R16_FLOAT) return 16;
    else if (dxgiFormat == DXGI_FORMAT_R16_UINT) return 16;
    else if (dxgiFormat == DXGI_FORMAT_R16_UNORM) return 16;
    else if (dxgiFormat == DXGI_FORMAT_R8_UNORM) return 8;
    else if (dxgiFormat == DXGI_FORMAT_A8_UNORM) return 8;

    // Unknown format !
    assert(false);
    return 32;
}

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
    unsigned max_count, IRHIGPUBuffer& arguments_buffer, unsigned arguments_buffer_offset)
{
    auto* dx_command_list = dynamic_cast<DX12CommandList&>(command_list).GetDXRCommandList();
    auto* dx_command_signature = dynamic_cast<DX12CommandSignature&>(command_signature).GetCommandSignature();

    auto* dx_arguments_buffer = dynamic_cast<DX12GPUBuffer&>(arguments_buffer).GetBuffer();
    
    dx_command_list->ExecuteIndirect(dx_command_signature, max_count, dx_arguments_buffer, arguments_buffer_offset, nullptr, 0 );
    
    return true; 
}

bool DX12Utils::ExecuteIndirect(IRHICommandList& command_list, IRHICommandSignature& command_signature,
                                unsigned max_count, IRHIGPUBuffer& arguments_buffer, unsigned arguments_buffer_offset, IRHIGPUBuffer& count_buffer,
                                unsigned count_buffer_offset)
{
    auto* dx_command_list = dynamic_cast<DX12CommandList&>(command_list).GetDXRCommandList();
    auto* dx_command_signature = dynamic_cast<DX12CommandSignature&>(command_signature).GetCommandSignature();

    auto* dx_arguments_buffer = dynamic_cast<DX12GPUBuffer&>(arguments_buffer).GetBuffer();
    auto* dx_count_buffer = dynamic_cast<DX12GPUBuffer&>(count_buffer).GetBuffer();
    
    dx_command_list->ExecuteIndirect(dx_command_signature, max_count, dx_arguments_buffer, arguments_buffer_offset, dx_count_buffer, count_buffer_offset );
    
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

bool DX12Utils::CopyTexture(IRHICommandList& commandList, IRHIRenderTarget& dst, IRHIRenderTarget& src)
{
    auto* dxCommandList = dynamic_cast<DX12CommandList&>(commandList).GetCommandList();
    
    D3D12_TEXTURE_COPY_LOCATION dstLocation;
    dstLocation.pResource = dynamic_cast<DX12RenderTarget&>(dst).GetResource();
    dstLocation.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX; 
    dstLocation.SubresourceIndex = 0;

    D3D12_TEXTURE_COPY_LOCATION srcLocation;
    srcLocation.pResource = dynamic_cast<DX12RenderTarget&>(src).GetResource();
    srcLocation.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX; 
    srcLocation.SubresourceIndex = 0;
    
    dxCommandList->CopyTextureRegion(&dstLocation, 0, 0, 0, &srcLocation, nullptr);
    
    return true;
}

bool DX12Utils::CopyBuffer(IRHICommandList& commandList, IRHIGPUBuffer& dst, size_t dst_offset, IRHIGPUBuffer& src,
    size_t src_offset, size_t size)
{
    auto* dxCommandList = dynamic_cast<DX12CommandList&>(commandList).GetCommandList();
    dxCommandList->CopyBufferRegion(dynamic_cast<DX12GPUBuffer&>(dst).GetBuffer(), dst_offset, dynamic_cast<DX12GPUBuffer&>(src).GetBuffer(), src_offset, size);
    
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
