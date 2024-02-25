#pragma once
#include <cassert>
#include <map>

#include "glTFUtils/glTFLog.h"
#include "glTFUtils/glTFUtils.h"
#include "glm.hpp"

typedef uint64_t RHIGPUDescriptorHandle;
typedef uint64_t RHICPUDescriptorHandle;
typedef unsigned RTID;

#define REGISTER_INDEX_TYPE unsigned 

enum RHICommonEnum
{
    RHI_Unknown = 123456,
};

enum class RHISampleCount
{
    SAMPLE_1_BIT,
    SAMPLE_2_BIT,
    SAMPLE_4_BIT,
    SAMPLE_8_BIT,
    SAMPLE_16_BIT,
    SAMPLE_32_BIT,
    SAMPLE_64_BIT,
};

enum class RHIAttachmentLoadOp
{
    LOAD_OP_LOAD,
    LOAD_OP_CLEAR,
    LOAD_OP_DONT_CARE,
    LOAD_OP_NONE,
};

enum class RHIAttachmentStoreOp
{
    STORE_OP_STORE,
    STORE_OP_DONT_CARE,
    STORE_OP_NONE,
};

enum class RHIResourceStateType
{
    STATE_UNKNOWN,
    STATE_COMMON,
    STATE_GENERIC_READ,
    STATE_COPY_SOURCE,
    STATE_COPY_DEST,
    STATE_VERTEX_AND_CONSTANT_BUFFER,
    STATE_INDEX_BUFFER,
    STATE_PRESENT,
    STATE_RENDER_TARGET,
    STATE_DEPTH_WRITE,
    STATE_DEPTH_READ,
    STATE_UNORDERED_ACCESS,
    STATE_NON_PIXEL_SHADER_RESOURCE,
    STATE_PIXEL_SHADER_RESOURCE,
    STATE_RAYTRACING_ACCELERATION_STRUCTURE,
};

enum class RHIDataFormat
{
    R8G8B8A8_UNORM,
    R8G8B8A8_UNORM_SRGB,
    B8G8R8A8_UNORM,
    B8G8R8A8_UNORM_SRGB,
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
    R32G32B32A32_UINT,
    R32_UINT,
    R32_TYPELESS,
    R16_FLOAT,
    R16_UNORM,
    R16_UINT,
    R8_UNORM,
    A8_UNORM,
    Unknown,
};

inline RHIDataFormat ConvertToSRGBFormat(RHIDataFormat format)
{
    switch (format)
    {
    case RHIDataFormat::R8G8B8A8_UNORM:
        return RHIDataFormat::R8G8B8A8_UNORM_SRGB;

    case RHIDataFormat::B8G8R8A8_UNORM:
        return RHIDataFormat::B8G8R8A8_UNORM_SRGB;
        
    default:
        GLTF_CHECK(false);
        break;
    }

    return format;
}

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

typedef RHIRect RHIScissorRectDesc;

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

    // UAV buffer desc
    unsigned stride;
    unsigned count;
    bool use_count_buffer;
    unsigned count_buffer_offset;
};
struct RHITextureDesc
{
public:
    RHITextureDesc();
    RHITextureDesc(const RHITextureDesc&) = delete;
    RHITextureDesc(RHITextureDesc&&) = delete;

    RHITextureDesc& operator=(const RHITextureDesc&) = delete;
    RHITextureDesc& operator=(RHITextureDesc&&) = delete;
    
    bool Init(unsigned width, unsigned height, RHIDataFormat format);
    
    unsigned char* GetTextureData() const { return m_texture_data.get(); }
    size_t GetTextureDataSize() const { return m_texture_data_size; }
    RHIDataFormat GetDataFormat() const { return m_texture_format; }
    unsigned GetTextureWidth() const { return m_texture_width; }
    unsigned GetTextureHeight() const { return m_texture_height; }

    void SetDataFormat(RHIDataFormat format) { m_texture_format = format;}
    
private:
    std::unique_ptr<unsigned char[]> m_texture_data;
    size_t m_texture_data_size;
    unsigned m_texture_width;
    unsigned m_texture_height;
    RHIDataFormat m_texture_format;
};

enum class RHISubPassBindPoint
{
    GRAPHICS,
    COMPUTE,
    RAYTRACING,
};

enum class RHIImageLayout
{
    UNDEFINED,
    GENERAL,
    COLOR_ATTACHMENT,
};

struct RHISubPassAttachment
{
    unsigned attachment_index;
    RHIImageLayout attachment_layout;
};

struct RHISubPassInfo
{
    RHISubPassBindPoint bind_point;
    std::vector<RHISubPassAttachment> color_attachments;
};

enum class RHIPipelineStage
{
    COLOR_ATTACHMENT_OUTPUT,
};

enum class RHIAccessFlags
{
    NONE,
    COLOR_ATTACHMENT_WRITE,
};

struct RHISubPassDependency
{
    unsigned src_subpass;
    unsigned dst_subpass;

    RHIPipelineStage src_stage;
    RHIAccessFlags src_access_flags; 

    RHIPipelineStage dst_stage;
    RHIAccessFlags dst_access_flags;
};

enum class RHIShaderType
{
    Vertex,
    Pixel,
    Compute,
    RayTracing,
    Unknown,
};

struct RHIShaderPreDefineMacros
{
    void AddMacro(const std::string& key, const std::string& value)
    {
        macroKey.push_back(key);
        macroValue.push_back(value);
    }

    void AddCBVRegisterDefine(const std::string& key, unsigned register_index, unsigned space = 0)
    {
        char format_string[32] = {'\0'};
        (void)snprintf(format_string, sizeof(format_string), "register(b%u, space%u)", register_index, space);
        AddMacro(key, format_string);
    }
    
    void AddSRVRegisterDefine(const std::string& key, unsigned register_index, unsigned space = 0)
    {
        char format_string[32] = {'\0'};
        (void)snprintf(format_string, sizeof(format_string), "register(t%u, space%u)", register_index, space);
        AddMacro(key, format_string);
    }
    
    void AddUAVRegisterDefine(const std::string& key, unsigned register_index, unsigned space = 0)
    {
        char format_string[32] = {'\0'};
        (void)snprintf(format_string, sizeof(format_string), "register(u%u, space%u)", register_index, space);
        AddMacro(key, format_string);
    }

    void AddSamplerRegisterDefine(const std::string& key, unsigned register_index, unsigned space = 0)
    {
        char format_string[32] = {'\0'};
        (void)snprintf(format_string, sizeof(format_string), "register(s%u, space%u)", register_index, space);
        AddMacro(key, format_string);
    }
    
    std::vector<std::string> macroKey;
    std::vector<std::string> macroValue;
};

enum class RHIRootParameterDescriptorRangeType
{
    CBV,
    SRV,
    UAV,
    Unknown,
};

enum class RHIShaderRegisterType
{
    b,
    t,
    u,
    Unknown,
};

struct RHIRootParameterDescriptorRangeDesc
{
    RHIRootParameterDescriptorRangeType type {RHIRootParameterDescriptorRangeType::Unknown} ;
    REGISTER_INDEX_TYPE base_register_index {0};
    unsigned space;
    size_t descriptor_count {0};
    bool is_bindless_range {false};
};

enum class RHIRootParameterType
{
    Constant, // 32 bit constant value
    CBV,
    SRV,
    UAV,
    DescriptorTable,
    Unknown, // Init as unknown, must be reset to other type
};

struct RootSignatureAllocation
{
    RootSignatureAllocation()
        : parameter_index(0)
        , register_index(0)
        , space(0)
    {
    }

    unsigned parameter_index;
    unsigned register_index;
    unsigned space;
};

enum class RHIStaticSamplerAddressMode
{
    Warp,
    Mirror,
    Clamp,
    Border,
    MirrorOnce,
    Unknown,
};

enum class RHIStaticSamplerFilterMode
{
    Point,
    Linear,
    Anisotropic,
    Unknown,
};

enum class RHIRootSignatureUsage
{
    None,
    Default,
    LocalRS,
};

struct RootSignatureStaticSamplerElement
{
    std::string sampler_name;
    unsigned sample_index;
    RHIStaticSamplerAddressMode address_mode;
    RHIStaticSamplerFilterMode filter_mode;
};

struct RootSignatureParameterElement
{
    std::string name;
    unsigned parameter_index;
    std::pair<unsigned, unsigned> register_range;
    unsigned space;
    unsigned constant_value_count;
    RHIRootParameterDescriptorRangeType table_type;
    bool is_bindless;
};

struct RootSignatureLayout
{
    RootSignatureLayout()
        : last_parameter_index(0)
    {
    }

    std::map<RHIRootParameterType, std::vector<RootSignatureParameterElement>> parameter_elements;
    std::vector<RootSignatureStaticSamplerElement> sampler_elements;

    std::map<RHIShaderRegisterType, unsigned> last_register_index;
    unsigned last_parameter_index;
    
    unsigned ParameterCount() const { return last_parameter_index; }
    unsigned SamplerCount() const {return sampler_elements.size(); }
};

struct RHIDepthStencilClearValue
{
    float clear_depth;
    unsigned char clear_stencil_value;
};

struct RHIRenderTargetClearValue
{
    RHIDataFormat clear_format;
    union 
    {
        glm::vec4 clear_color;
        RHIDepthStencilClearValue clearDS;
    };
};

enum class RHIRenderTargetType
{
    RTV,
    DSV,
    Unknown,
};

struct RHIRenderTargetDesc
{
    unsigned width;
    unsigned height;
    bool isUAV;
    RHIRenderTargetClearValue clearValue;
    std::string name;
};

struct RHIPipelineInputLayout
{
    std::string semanticName;
    unsigned semanticIndex;
    RHIDataFormat format;
    unsigned alignedByteOffset;

    unsigned slot;

    bool IsVertexData() const {return slot == 0; }
};

enum class RHICullMode
{
    NONE,
    CW,
    CCW,
};

enum class RHIDepthStencilMode
{
    DEPTH_READ,
    DEPTH_WRITE,
};

enum class RHIPipelineType
{
    Graphics,
    Compute,
    RayTracing,
    //Copy,
    Unknown,
};

enum class RHIBufferResourceType
{
    Buffer,
    Tex1D,
    Tex2D,
    Tex3D,
};

enum class RHIBufferType
{
    Default,
    Upload,
};

enum class RHIBufferUsage
{
    NONE,
    ALLOW_UNORDER_ACCESS,
};

struct RHIBufferDesc
{
    std::wstring name {'\0'};
    size_t width {0};
    size_t height {0};
    size_t depth {0};
    
    RHIBufferType type;
    RHIDataFormat resource_data_type;
    RHIBufferResourceType resource_type;
    RHIResourceStateType state {RHIResourceStateType::STATE_COMMON};
    RHIBufferUsage usage {RHIBufferUsage::NONE};
    
    size_t alignment {0};
};

struct RHIDescriptorHeapDesc
{
    unsigned maxDescriptorCount;
    RHIDescriptorHeapType type;
    bool shaderVisible;
};

struct RHIIndirectArgumentDraw
{
    unsigned VertexCountPerInstance;
    unsigned InstanceCount;
    unsigned StartVertexLocation;
    unsigned StartInstanceLocation;
};

struct RHIIndirectArgumentDrawIndexed
{
    unsigned IndexCountPerInstance;
    unsigned InstanceCount;
    unsigned StartIndexLocation;
    unsigned BaseVertexLocation;
    unsigned StartInstanceLocation;
};

struct RHIIndirectArgumentDispatch
{
    unsigned ThreadGroupCountX;
    unsigned ThreadGroupCountY;
    unsigned ThreadGroupCountZ;
};

struct RHIIndirectArgumentView
{
    RHIGPUDescriptorHandle handle;
};

enum class RHIIndirectArgType
{
    Draw,
    DrawIndexed,
    Dispatch,
    VertexBufferView,
    IndexBufferView,
    Constant,
    ConstantBufferView,
    ShaderResourceView,
    UnOrderAccessView,
    DispatchRays,
    DispatchMesh
};

struct RHIIndirectArgumentDesc
{
    RHIIndirectArgType type;

    union 
    {
        struct 
        {
            unsigned slot;    
        } vertex_buffer;

        struct 
        {
            unsigned root_parameter_index;
            unsigned dest_offset_in_32bit_value;
            unsigned num_32bit_values_to_set;
        } constant;

        struct
        {
            unsigned root_parameter_index;
        } constant_buffer_view;
        
        struct
        {
            unsigned root_parameter_index;
        } shader_resource_view;
        
        struct
        {
            unsigned root_parameter_index;
        } unordered_access_view;
    } desc;
};

struct RHICommandSignatureDesc
{
    std::vector<RHIIndirectArgumentDesc> m_indirect_arguments;
    unsigned stride;
};

enum class RHICommandAllocatorType
{
    DIRECT,
    COMPUTE,
    COPY,
    BUNDLE,
    UNKNOWN,
};

#define THROW_IF_FAILED(x) \
    { \
    HRESULT result = (x); \
    if (FAILED(result)) \
    { \
        LOG_FORMAT_FLUSH("[ERROR] Failed with error code: %u\n", result); \
        throw std::runtime_error("[ERROR] Detected runtime error in call"); \
    } \
    }

#define RETURN_IF_FALSE(x) \
    if (!(x)) \
    { \
    assert(false); return false; \
    }

#define GPU_BUFFER_HANDLE_TYPE unsigned long long

// get the number of bits per pixel for a dxgi format
inline unsigned GetRHIDataFormatBitsPerPixel(const RHIDataFormat& RHIDataFormat)
{
    if (RHIDataFormat == RHIDataFormat::R32G32B32A32_FLOAT) return 128;
    else if (RHIDataFormat == RHIDataFormat::R32G32B32A32_UINT) return 128;
    else if (RHIDataFormat == RHIDataFormat::R32G32B32_FLOAT) return 96;
    else if (RHIDataFormat == RHIDataFormat::R32G32_FLOAT) return 64;
    else if (RHIDataFormat == RHIDataFormat::R16G16B16A16_FLOAT) return 64;
    else if (RHIDataFormat == RHIDataFormat::R16G16B16A16_UNORM) return 64;
    else if (RHIDataFormat == RHIDataFormat::R8G8B8A8_UNORM) return 32;
    else if (RHIDataFormat == RHIDataFormat::R8G8B8A8_UNORM_SRGB) return 32;
    else if (RHIDataFormat == RHIDataFormat::B8G8R8A8_UNORM) return 32;
    else if (RHIDataFormat == RHIDataFormat::B8G8R8A8_UNORM_SRGB) return 32;
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