#pragma once

#include <d3d12.h>
#include <dxgiformat.h>

#include "glTFRHI/RHIInterface/IRHIRenderTarget.h"

class DX12ConverterUtils
{
public:
    // get the number of bits per pixel for a dxgi format
    static int GetDXGIFormatBitsPerPixel(const DXGI_FORMAT& dxgiFormat);
    
    static DXGI_FORMAT ConvertToDXGIFormat(RHIDataFormat format);
    static D3D12_HEAP_TYPE ConvertToHeapType(RHIBufferType type);
    static D3D12_RESOURCE_STATES ConvertToResourceState(RHIResourceStateType state);
    static D3D_PRIMITIVE_TOPOLOGY ConvertToPrimitiveTopologyType(RHIPrimitiveTopologyType type);
    static D3D12_DESCRIPTOR_HEAP_TYPE ConvertToDescriptorHeapType(RHIDescriptorHeapType type);
    static D3D12_SRV_DIMENSION ConvertToSRVDimensionType(RHIResourceDimension type);
    static D3D12_UAV_DIMENSION ConvertToUAVDimensionType(RHIResourceDimension type);
    static D3D12_CLEAR_VALUE ConvertToD3DClearValue(RHITextureClearValue clear_value);
    static D3D12_INDIRECT_ARGUMENT_TYPE ConvertToIndirectArgumentType(RHIIndirectArgType type);
    static D3D12_INDIRECT_ARGUMENT_DESC ConvertToIndirectArgumentDesc(const RHIIndirectArgumentDesc& desc);
    static D3D12_RESOURCE_FLAGS ConvertToResourceFlags(RHIResourceUsageFlags usage);
};
