#pragma once
#include <cassert>
#include <stdexcept>

enum RHICommonEnum
{
    RHI_Unknown = 123456,
};

enum class RHIResourceStateType
{
    COPY_SOURCE,
    COPY_DEST,
    VERTEX_AND_CONSTANT_BUFFER,
    INDEX_BUFFER,
    PRESENT,
    RENDER_TARGET,
    DEPTH_WRITE,
    DEPTH_READ,
    UNORDER_ACCESS,
    PIXEL_SHADER_RESOURCE,
};

enum class RHIDataFormat
{
    R8G8B8A8_UNORM,
    R8G8B8A8_UNORM_SRGB,
    B8G8R8A8_UNORM,
    B8G8R8X8_UNORM,
    R32G32_FLOAT,
    R32G32B32_FLOAT,
    R32G32B32A32_FLOAT,
    R16G16B16A16_FLOAT,
    R16G16B16A16_UNORM,
    R10G10B10A2_UNORM,
    R10G10B10_XR_BIAS_A2_UNORM,
    B5G5R5A1_UNORM,
    B5G6R5_UNORM,
    D32_FLOAT,
    R32_FLOAT,
    R32_UINT,
    R32_TYPELESS,
    R16_FLOAT,
    R16_UNORM,
    R16_UINT,
    R8_UNORM,
    A8_UNORM,
    Unknown,
};

enum class RHIPrimitiveTopologyType
{
    TRIANGLELIST,
};

enum class RHIDescriptorHeapType
{
    CBV_SRV_UAV,
    SAMPLER,
    RTV,
    DSV,
};

struct RHIRect
{
    long    left;
    long    top;
    long    right;
    long    bottom;
};

struct RHIViewportDesc
{
    float TopLeftX;
    float TopLeftY;
    float Width;
    float Height;
    float MinDepth;
    float MaxDepth;
};

struct RHIConstantBufferViewDesc
{
    uint64_t bufferLocation;
    size_t bufferSize;
};

enum class RHIResourceDimension
{
    UNKNOWN,
    BUFFER,
    TEXTURE1D,
    TEXTURE1DARRAY,
    TEXTURE2D,
    TEXTURE2DARRAY,
    TEXTURE2DMS,
    TEXTURE2DMSARRAY,
    TEXTURE3D,
    TEXTURECUBE,
    TEXTURECUBEARRAY,
};

struct RHIShaderResourceViewDesc
{
    RHIDataFormat format;
    RHIResourceDimension dimension;
    
    // TODO: figure out this member represent what?
    //unsigned shader4ComponentMapping;
};

typedef RHIRect RHIScissorRectDesc;
typedef uint64_t RHIGPUDescriptorHandle;
typedef uint64_t RHICPUDescriptorHandle;

#define REGISTER_INDEX_TYPE unsigned 

#define THROW_IF_FAILED(x) \
    if (FAILED((x))) \
    { \
        throw std::runtime_error("[ERROR] Detected runtime error in call"); \
    }

#define RETURN_IF_FALSE(x) \
    if (!(x)) \
    { assert(false); return false; }

#define GPU_BUFFER_HANDLE_TYPE unsigned long long

// get the number of bits per pixel for a dxgi format
inline unsigned GetRHIDataFormatBitsPerPixel(const RHIDataFormat& RHIDataFormat)
{
    if (RHIDataFormat == RHIDataFormat::R32G32B32A32_FLOAT) return 128;
    else if (RHIDataFormat == RHIDataFormat::R32G32B32_FLOAT) return 96;
    else if (RHIDataFormat == RHIDataFormat::R32G32_FLOAT) return 64;
    else if (RHIDataFormat == RHIDataFormat::R16G16B16A16_FLOAT) return 64;
    else if (RHIDataFormat == RHIDataFormat::R16G16B16A16_UNORM) return 64;
    else if (RHIDataFormat == RHIDataFormat::R8G8B8A8_UNORM) return 32;
    else if (RHIDataFormat == RHIDataFormat::B8G8R8A8_UNORM) return 32;
    else if (RHIDataFormat == RHIDataFormat::B8G8R8X8_UNORM) return 32;
    else if (RHIDataFormat == RHIDataFormat::R10G10B10_XR_BIAS_A2_UNORM) return 32;

    else if (RHIDataFormat == RHIDataFormat::R10G10B10A2_UNORM) return 32;
    else if (RHIDataFormat == RHIDataFormat::B5G5R5A1_UNORM) return 16;
    else if (RHIDataFormat == RHIDataFormat::B5G6R5_UNORM) return 16;
    else if (RHIDataFormat == RHIDataFormat::R32_FLOAT) return 32;
    else if (RHIDataFormat == RHIDataFormat::R32_UINT) return 32;
    else if (RHIDataFormat == RHIDataFormat::R32_TYPELESS) return 32;
    else if (RHIDataFormat == RHIDataFormat::R16_FLOAT) return 16;
    else if (RHIDataFormat == RHIDataFormat::R16_UNORM) return 16;
    else if (RHIDataFormat == RHIDataFormat::R16_UINT) return 16;
    else if (RHIDataFormat == RHIDataFormat::R8_UNORM) return 8;
    else if (RHIDataFormat == RHIDataFormat::A8_UNORM) return 8;

    // Unknown format !
    assert(false);
    return 32;
}