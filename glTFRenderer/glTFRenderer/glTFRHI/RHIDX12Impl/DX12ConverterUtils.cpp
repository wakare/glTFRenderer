#include "DX12ConverterUtils.h"

#define CONVERT_DXGI_FORMAT_CASE(RHIFormat) case RHIDataFormat::##RHIFormat: return DXGI_FORMAT_##RHIFormat;

int DX12ConverterUtils::GetDXGIFormatBitsPerPixel(const DXGI_FORMAT& dxgiFormat)
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

D3D12_CLEAR_VALUE DX12ConverterUtils::ConvertToD3DClearValue(RHITextureClearValue clear_value)
{
    D3D12_CLEAR_VALUE dx_clear_value = {};
    dx_clear_value.Format = ConvertToDXGIFormat(clear_value.clear_format);
    memcpy(dx_clear_value.Color, &clear_value.clear_color, sizeof(clear_value.clear_color));
    dx_clear_value.DepthStencil.Depth = clear_value.clear_depth_stencil.clear_depth;
    dx_clear_value.DepthStencil.Stencil = clear_value.clear_depth_stencil.clear_stencil_value;

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