#pragma once

/*
#ifdef RHICORE_EXPORTS
#define RHICORE_API __declspec(dllexport)
#else
#define RHICORE_API __declspec(dllimport)
#endif
*/
#define RHICORE_API

#include <cassert>
#include <map>
#include <wincodec.h>

#include "RendererCommon.h"

class IRHITextureDescriptorAllocation;
class IRHIFrameBuffer;
class IRHIRenderPass;
class IRHISemaphore;
class glTFRenderResourceManager;
struct ImageLoadResult;
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
    STATE_UNDEFINED,
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
    STATE_ALL_SHADER_RESOURCE,
    STATE_RAYTRACING_ACCELERATION_STRUCTURE,
};

enum class RHIDataFormat
{
    // Float type
    R32G32_FLOAT,
    R32G32B32_FLOAT,
    R32G32B32A32_FLOAT,
    R16G16B16A16_FLOAT,
    R16G16B16A16_UNORM,
    R10G10B10A2_UNORM,
    R10G10B10_XR_BIAS_A2_UNORM,
    R8G8B8A8_UNORM,
    R8G8B8A8_UNORM_SRGB,
    B8G8R8A8_UNORM,
    B8G8R8A8_UNORM_SRGB,
    B8G8R8X8_UNORM,
    B5G5R5A1_UNORM,
    B5G6R5_UNORM,
    D32_FLOAT,
    R32_FLOAT,
    R16_FLOAT,
    R16_UNORM,
    R8_UNORM,
    A8_UNORM,
    
    // Uint type
    R32G32B32A32_UINT,
    R16G16B16A16_UINT,
    R32_UINT,
    R32_TYPELESS,
    R16_UINT,
    
    D32_SAMPLE_RESERVED,
    
    UNKNOWN,
};

inline bool IsFloatDataFormat(RHIDataFormat format)
{
    switch (format) {
    case RHIDataFormat::R32G32_FLOAT:
    case RHIDataFormat::R32G32B32_FLOAT:
    case RHIDataFormat::R32G32B32A32_FLOAT:
    case RHIDataFormat::R16G16B16A16_FLOAT:
    case RHIDataFormat::R16G16B16A16_UNORM:
    case RHIDataFormat::R10G10B10A2_UNORM:
    case RHIDataFormat::R10G10B10_XR_BIAS_A2_UNORM:
    case RHIDataFormat::R8G8B8A8_UNORM:
    case RHIDataFormat::R8G8B8A8_UNORM_SRGB:
    case RHIDataFormat::B8G8R8A8_UNORM:
    case RHIDataFormat::B8G8R8A8_UNORM_SRGB:
    case RHIDataFormat::B8G8R8X8_UNORM:
    case RHIDataFormat::B5G5R5A1_UNORM:
    case RHIDataFormat::B5G6R5_UNORM:
    case RHIDataFormat::D32_FLOAT:
    case RHIDataFormat::R32_FLOAT:
    case RHIDataFormat::R16_FLOAT:
    case RHIDataFormat::R16_UNORM:
    case RHIDataFormat::R8_UNORM:
    case RHIDataFormat::A8_UNORM:
        return true;
    }

    return false;
}

inline bool IsUintDataFormat(RHIDataFormat format)
{
    return !IsFloatDataFormat(format);
}

inline bool IsDepthStencilFormat(RHIDataFormat format)
{
    switch (format) {
    case RHIDataFormat::D32_FLOAT:
    case RHIDataFormat::D32_SAMPLE_RESERVED:
        return true;
    }
    return false;
}

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
    CBV_SRV_UAV_GPU,
    CBV_SRV_UAV_CPU,
    SAMPLER,
    RTV,
    DSV,
    UNKNOWN,
};

struct RHICORE_API RHIRect
{
    unsigned    left;
    unsigned    top;
    unsigned    right;
    unsigned    bottom;
};

typedef RHIRect RHIScissorRectDesc;

struct RHICORE_API RHIViewportDesc
{
    float top_left_x;
    float top_left_y;
    float width;
    float height;
    float min_depth;
    float max_depth;
};

struct RHICORE_API RHIConstantBufferViewDesc
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

enum class RHIViewType
{
    // GPU descriptor
    RVT_CBV,
    RVT_SRV,
    RVT_UAV,
    // CPU descriptor
    RVT_RTV,
    RVT_DSV,
    // Vertex buffer view and index buffer view
    RVT_VBV,
    RVT_IBV,
};

struct RHICORE_API RHIDescriptorDesc
{
    RHIDataFormat m_format;
    RHIResourceDimension m_dimension;
    RHIViewType m_view_type;

    RHIDescriptorDesc(RHIDataFormat format, RHIResourceDimension dimension, RHIViewType view_type)
        : m_format(format)
        , m_dimension(dimension)
        , m_view_type(view_type)
    {}

    bool IsBufferDescriptor() const {return m_dimension == RHIResourceDimension::BUFFER; }
    bool IsTextureDescriptor() const {return !IsBufferDescriptor(); }
    bool IsVBOrIB() const {return m_view_type == RHIViewType::RVT_VBV || m_view_type == RHIViewType::RVT_IBV; }
    
    virtual ~RHIDescriptorDesc() = default;
    
    bool operator==(const RHIDescriptorDesc& other) const
    {
        return
            m_format == other.m_format &&
            m_dimension == other.m_dimension &&
            m_view_type == other.m_view_type;
    }
};

struct RHICORE_API RHIUAVStructuredBufferDesc
{
    unsigned stride {0};
    unsigned count {0};
    bool is_structured_buffer {true};
    bool use_count_buffer {false};
    unsigned count_buffer_offset {0};
    
    bool operator==(const RHIUAVStructuredBufferDesc& other) const
    {
        return
            stride == other.stride &&
            count == other.count &&
            is_structured_buffer == other.is_structured_buffer &&
            use_count_buffer == other.use_count_buffer &&
            count_buffer_offset == other.count_buffer_offset;
    }
};

struct RHISRVStructuredBufferDesc
{
    unsigned stride {0};
    unsigned count {0};
    bool is_structured_buffer {true};

    bool operator==(const RHISRVStructuredBufferDesc& other) const
    {
        return
            stride == other.stride &&
            count == other.count &&
            is_structured_buffer == other.is_structured_buffer;
    }
};

struct RHICORE_API RHIBufferDescriptorDesc : RHIDescriptorDesc
{
    RHIBufferDescriptorDesc(RHIDataFormat format, RHIViewType view_type, size_t size, size_t offset, const RHIUAVStructuredBufferDesc& uav_structured_desc)
            : RHIDescriptorDesc(format, RHIResourceDimension::BUFFER, view_type)
            , m_size(size)
            , m_offset(offset)
            , m_uav_structured_buffer_desc(uav_structured_desc)
    {
    }

    RHIBufferDescriptorDesc(RHIDataFormat format, RHIViewType view_type, size_t size, size_t offset, const RHISRVStructuredBufferDesc& srv_structured_desc)
            : RHIDescriptorDesc(format, RHIResourceDimension::BUFFER, view_type)
            , m_size(size)
            , m_offset(offset)
            , m_srv_structured_buffer_desc(srv_structured_desc)
    {
    }
    
    RHIBufferDescriptorDesc(RHIDataFormat format, RHIViewType view_type, size_t size, size_t offset)
        : RHIDescriptorDesc(format, RHIResourceDimension::BUFFER, view_type)
        , m_size(size)
        , m_offset(offset)
        , m_uav_structured_buffer_desc({})
    {
    }
    
    size_t m_size;
    size_t m_offset;
    
    union 
    {
        RHISRVStructuredBufferDesc m_srv_structured_buffer_desc;
        RHIUAVStructuredBufferDesc m_uav_structured_buffer_desc;
    };

    bool operator==(const RHIBufferDescriptorDesc& other) const
    {
        if (!RHIDescriptorDesc::operator==(other))
        {
            return false;
        }

        if (m_view_type == RHIViewType::RVT_SRV)
        {
            return m_srv_structured_buffer_desc == other.m_srv_structured_buffer_desc;
        }
        else if (m_view_type == RHIViewType::RVT_UAV)
        {
            return m_uav_structured_buffer_desc == other.m_uav_structured_buffer_desc;   
        }
        
        return true;
    }
};

struct RHICORE_API RHITextureDescriptorDesc : RHIDescriptorDesc
{
    RHITextureDescriptorDesc(RHIDataFormat format, RHIResourceDimension dimension, RHIViewType view_type)
        : RHIDescriptorDesc(format, dimension, view_type)
    {}
};

enum class RHICORE_API RHISubPassBindPoint
{
    GRAPHICS,
    COMPUTE,
    RAYTRACING,
};

enum class RHICORE_API RHIImageLayout
{
    UNDEFINED,
    GENERAL,
    COLOR_ATTACHMENT,
};

struct RHICORE_API RHISubPassAttachment
{
    unsigned attachment_index;
    RHIImageLayout attachment_layout;
};

struct RHICORE_API RHISubPassInfo
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

struct RHICORE_API RHISubPassDependency
{
    unsigned src_subpass;
    unsigned dst_subpass;

    RHIPipelineStage src_stage;
    RHIAccessFlags src_access_flags; 

    RHIPipelineStage dst_stage;
    RHIAccessFlags dst_access_flags;
};

enum class RHIDescriptorRangeType
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
    s,
    Unknown,
};

struct RHICORE_API RHIDescriptorRangeDesc
{
    RHIDescriptorRangeType type {RHIDescriptorRangeType::Unknown} ;
    REGISTER_INDEX_TYPE base_register_index {0};
    unsigned space;
    size_t descriptor_count {0};
    bool is_bindless_range {false};
    bool is_buffer {false};
};

enum class RHIRootParameterType
{
    Constant, // 32 bit constant value
    CBV,
    SRV,
    UAV,
    DescriptorTable,
    Sampler,
    Unknown, // Init as unknown, must be reset to other type
};

struct RHICORE_API RootSignatureAllocation
{
    RootSignatureAllocation()
        : parameter_name("")
        , global_parameter_index(0)
        , local_space_parameter_index(0)
        , register_begin_index(0)
        , register_end_index(0)
        , space(0)
        , bindless_descriptor(false)
        , type(RHIRootParameterType::Unknown)
        , register_type(RHIShaderRegisterType::Unknown)
    {
    }

    void AddShaderDefine(RHIShaderPreDefineMacros& out_shader_macros) const;

    std::string parameter_name;
    unsigned global_parameter_index;
    unsigned local_space_parameter_index;
    unsigned register_begin_index;
    unsigned register_end_index;
    unsigned space;
    bool bindless_descriptor;
    RHIRootParameterType type;
    RHIShaderRegisterType register_type;
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

struct RHICORE_API RootSignatureStaticSamplerElement
{
    std::string sampler_name;
    unsigned register_space;
    unsigned sample_index;
    RHIStaticSamplerAddressMode address_mode;
    RHIStaticSamplerFilterMode filter_mode;
};

struct RHICORE_API RootSignatureParameterElement
{
    std::string name;
    unsigned global_parameter_index;
    unsigned local_space_parameter_index;
    std::pair<unsigned, unsigned> register_range;
    unsigned space;
    unsigned constant_value_count;
    RHIDescriptorRangeType table_type;
    bool is_bindless;
    bool is_buffer;
};

struct RHICORE_API RootSignatureLayout
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

struct RHICORE_API RHIDepthStencilClearValue
{
    float clear_depth;
    unsigned char clear_stencil_value;

    bool operator==(const RHIDepthStencilClearValue& other) const
    {
        return clear_depth == other.clear_depth &&
            clear_stencil_value == other.clear_stencil_value;
    }
};

struct RHITextureClearValue
{
    RHIDataFormat clear_format;
    union 
    {
        float clear_color[4];
        RHIDepthStencilClearValue clear_depth_stencil;
    };

    bool operator==(const RHITextureClearValue& rhs) const
    {
        return clear_format == rhs.clear_format &&
            clear_color == rhs.clear_color &&
            clear_depth_stencil == rhs.clear_depth_stencil;
    }
};

enum class RHIRenderTargetType
{
    RTV,
    DSV,
    Unknown,
};

enum RHIResourceUsageFlags
{
    RUF_NONE                =  0x0u,
    RUF_ALLOW_CBV           =  0x1u,
    RUF_ALLOW_UAV           =  0x1u << 1,
    RUF_ALLOW_SRV           =  0x1u << 2,
    
    // TEXTURE
    RUF_ALLOW_DEPTH_STENCIL =  0x1u << 3,
    RUF_ALLOW_RENDER_TARGET =  0x1u << 4,
    
    // BUFFER
    RUF_VERTEX_BUFFER       =  0x1u << 5,
    RUF_INDEX_BUFFER        =  0x1u << 6,
    RUF_INDIRECT_BUFFER     =  0x1u << 7,

    // COMMON
    RUF_TRANSFER_SRC        =  0x1u << 8,
    RUF_TRANSFER_DST        =  0x1u << 9,

    // Others
    RUF_READBACK            =  0x1u << 10,
    RUF_CONTAINS_MIPMAP     =  0x1u << 11,
    RUF_SUPPORT_CLEAR_VALUE =  0x1u << 12,
};

struct RHICORE_API RHITextureDesc
{
    RHITextureDesc() = default;
    ~RHITextureDesc() = default;
    
    RHITextureDesc(const RHITextureDesc&) noexcept;
    RHITextureDesc& operator=(const RHITextureDesc&) noexcept;

    RHITextureDesc(RHITextureDesc&& desc) noexcept;
    RHITextureDesc& operator=(RHITextureDesc&& desc) noexcept;

    RHITextureDesc(std::string name, unsigned width, unsigned height, RHIDataFormat format, RHIResourceUsageFlags usage, const RHITextureClearValue& clear_value);
    
    bool InitWithLoadedData(const ImageLoadResult& image_load_result);
    
    // No data copy!!
    bool InitWithoutCopyData(const RHITextureDesc& other);

    // Texture data attribute
    bool SetTextureData(const char* data, size_t byte_size);
    bool HasTextureData() const {return m_texture_data != nullptr; }
    std::shared_ptr<unsigned char[]> GetTextureData() const { return m_texture_data; }
    size_t GetTextureDataSize() const { return m_texture_data_size; }

    // Common attribute
    RHIDataFormat GetDataFormat() const { return m_texture_format; }
    unsigned GetTextureWidth(unsigned mip = 0) const { return m_texture_width >> mip; }
    unsigned GetTextureHeight(unsigned mip = 0) const { return m_texture_height >> mip; }

    void SetDataFormat(RHIDataFormat format) { m_texture_format = format;}
    static RHIDataFormat ConvertToRHIDataFormat(const WICPixelFormatGUID& wicFormatGUID);

    bool HasUsage(RHIResourceUsageFlags usage) const;
    RHIResourceUsageFlags GetUsage() const {return m_usage; }
    
    const RHITextureClearValue& GetClearValue() const;
    const std::string& GetName() const;
    void SetName(const std::string& name);

    static RHITextureDesc MakeFullScreenTextureDesc(const std::string& name, RHIDataFormat format, RHIResourceUsageFlags usage, const RHITextureClearValue& clear_value, unsigned
                                                    width, unsigned height);
    
    static RHITextureDesc MakeDepthTextureDesc(unsigned width, unsigned height);
    static RHITextureDesc MakeScreenUVOffsetTextureDesc(unsigned width, unsigned height);

    static RHITextureDesc MakeBasePassAlbedoTextureDesc(unsigned width, unsigned height);
    static RHITextureDesc MakeBasePassNormalTextureDesc(unsigned width, unsigned height);
    static RHITextureDesc MakeLightingPassOutputTextureDesc(unsigned width, unsigned height);
    static RHITextureDesc MakeRayTracingSceneOutputTextureDesc(unsigned width, unsigned height);
    static RHITextureDesc MakeRayTracingPassReSTIRSampleOutputDesc(unsigned width, unsigned height);
    static RHITextureDesc MakeComputePassRayTracingPostProcessOutputDesc(unsigned width, unsigned height);
    static RHITextureDesc MakeShadowPassOutputDesc(unsigned width, unsigned height);
    
    static RHITextureDesc MakeVirtualTextureFeedbackDesc(unsigned width, unsigned height, unsigned feed_back_size_x, unsigned feed_back_size_y);
    
private:
    std::string m_name;
    std::shared_ptr<unsigned char[]> m_texture_data {nullptr};
    size_t m_texture_data_size {0};
    unsigned m_texture_width {0};
    unsigned m_texture_height {0};
    RHIDataFormat m_texture_format {RHIDataFormat::UNKNOWN};
    RHIResourceUsageFlags m_usage {};
    RHITextureClearValue m_clear_value {};
};

enum RHIAttributeFrequency
{
    PER_VERTEX,
    PER_INSTANCE,
};

struct RHIPipelineInputLayout
{
    // DX streaming usage
    std::string semantic_name;
    unsigned semantic_index;
    
    RHIDataFormat format;
    unsigned aligned_byte_offset;
    unsigned slot;
    
    RHIAttributeFrequency frequency;

    // VK streaming usage
    unsigned layout_location;

    bool IsVertexData() const { return frequency == PER_VERTEX; }
    bool IsInstanceData() const { return frequency == PER_INSTANCE; }
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
    DEPTH_DONT_CARE,
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
    Readback,
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
    RHIResourceUsageFlags usage {};
    size_t alignment {0};
    RHITextureClearValue clear_value{};

    bool operator==(const RHIBufferDesc& rhs) const
    {
        return
            width == rhs.width && height == rhs.height && depth == rhs.depth &&
            type == rhs.type &&
            resource_data_type == rhs.resource_data_type &&
            resource_type == rhs.resource_type &&
            usage == rhs.usage &&
            state == rhs.state &&
            alignment == rhs.alignment 
            //&& clear_value == rhs.clear_value
        ;
    }
};

struct RHIDescriptorHeapDesc
{
    unsigned max_descriptor_count;
    RHIDescriptorHeapType type;
    bool shader_visible;
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
inline unsigned GetBitPerPixelByFormat(const RHIDataFormat& RHIDataFormat)
{
    if (RHIDataFormat == RHIDataFormat::R32G32B32A32_FLOAT) return 128;
    else if (RHIDataFormat == RHIDataFormat::R32G32B32A32_UINT) return 128;
    else if (RHIDataFormat == RHIDataFormat::R32G32B32_FLOAT) return 96;
    else if (RHIDataFormat == RHIDataFormat::R32G32_FLOAT) return 64;
    else if (RHIDataFormat == RHIDataFormat::R16G16B16A16_FLOAT) return 64;
    else if (RHIDataFormat == RHIDataFormat::R16G16B16A16_UINT) return 64;
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

inline unsigned GetBytePerPixelByFormat(const RHIDataFormat& RHIDataFormat)
{
    return GetBitPerPixelByFormat(RHIDataFormat) / 8;
}

struct RHIExecuteCommandListWaitInfo
{
    const IRHISemaphore* m_wait_semaphore;
    RHIPipelineStage wait_stage;
};

struct RHIExecuteCommandListContext
{
    std::vector<RHIExecuteCommandListWaitInfo> wait_infos;
    std::vector<const IRHISemaphore*> sign_semaphores;
};

struct RHIBeginRenderPassInfo
{
    const IRHIRenderPass* render_pass;
    const IRHIFrameBuffer* frame_buffer;

    int offset_x;
    int offset_y;
    unsigned width;
    unsigned height;
};

struct RHIBeginRenderingInfo
{
    std::vector<IRHITextureDescriptorAllocation*> m_render_targets;
    int rendering_area_offset_x{0};
    int rendering_area_offset_y{0};
    unsigned rendering_area_width {0};
    unsigned rendering_area_height {0};
    bool enable_depth_write {false};
    bool clear_render_target {false};
    bool clear_depth_stencil {true};
};

struct RHITextureUploadInfo
{
    std::shared_ptr<unsigned char[]> data;
    size_t data_size {0};
    int dst_x {0};
    int dst_y {0};
    unsigned width {0};
    unsigned height {0};
};

struct RHITextureMipUploadInfo : public RHITextureUploadInfo
{
    unsigned int mip_level{0};
};

struct RHICopyTextureInfo
{
    int dst_x{0};
    int dst_y{0};
    
    unsigned int src_mip_level{0};
    unsigned int dst_mip_level{0};

    // Only used in uploading buffer to texture
    unsigned int copy_width{0};
    unsigned int copy_height{0};

    unsigned row_pitch{0};
};

struct RHIMipMapCopyRequirements
{
    std::vector<size_t> row_byte_size;
    std::vector<size_t> row_pitch;
    size_t total_size;
};

enum class VertexAttributeType
{
    // Vertex
    VERTEX_POSITION,
    VERTEX_NORMAL,
    VERTEX_TANGENT,
    VERTEX_TEXCOORD0,

    // Instance
    INSTANCE_MAT_0,
    INSTANCE_MAT_1,
    INSTANCE_MAT_2,
    INSTANCE_MAT_3,
    INSTANCE_CUSTOM_DATA,
};

struct VertexAttributeElement
{
    VertexAttributeType type;
    unsigned byte_size;
};


struct VertexLayoutDeclaration
{
    std::vector<VertexAttributeElement> elements;

    bool HasAttribute(VertexAttributeType attribute_type) const
    {
        for (const auto& element : elements)
        {
            if (element.type == attribute_type)
            {
                return true;
            }
        }
        return false;
    }
    
    size_t GetVertexStrideInBytes() const
    {
        size_t stride = 0;
        for (const auto& element : elements)
        {
            stride += element.byte_size;
        }

        return stride;
    }

    bool operator==(const VertexLayoutDeclaration& lhs) const
    {
        if (elements.size() != lhs.elements.size())
        {
            return false;
        }

        for (size_t i = 0; i < elements.size(); ++i)
        {
            if (elements[i].type != lhs.elements[i].type ||
                elements[i].byte_size != lhs.elements[i].byte_size)
            {
                return false;
            }
        }

        return true;
    }
};

struct VertexBufferData
{
    VertexLayoutDeclaration layout;
    
    std::unique_ptr<char[]> data;
    size_t byte_size;
    size_t vertex_count;

    bool GetVertexAttributeDataByIndex(VertexAttributeType type, unsigned index, void* out_data, size_t& out_attribute_size) const
    {
        const char* start_data = data.get() + index * layout.GetVertexStrideInBytes();
        for (unsigned i = 0; i < layout.elements.size(); ++i)
        {
            if (type == layout.elements[i].type)
            {
                memcpy(out_data, start_data, layout.elements[i].byte_size);
                out_attribute_size = layout.elements[i].byte_size;
                return true;
            }

            start_data += layout.elements[i].byte_size;
        }

        out_attribute_size = 0;
        return false;
    }
};


struct IndexBufferData
{
    RHIDataFormat format;
    
    std::unique_ptr<char[]> data;
    size_t byte_size;
    size_t index_count;

    unsigned GetStride() const;
    unsigned GetIndexByOffset(size_t offset) const;
};