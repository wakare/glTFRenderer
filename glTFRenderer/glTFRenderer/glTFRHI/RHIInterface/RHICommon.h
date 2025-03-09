#pragma once
#include <cassert>
#include <map>
#include <wincodec.h>

#include "RendererCommon.h"
#include "glm.hpp"

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
    STATE_ALL_SHADER_RESOURCE,
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
    R16G16B16A16_UINT,
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
    D32_SAMPLE_RESERVED,
    UNKNOWN,
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
    UNKNOWN,
};

struct RHIRect
{
    unsigned    left;
    unsigned    top;
    unsigned    right;
    unsigned    bottom;
};

typedef RHIRect RHIScissorRectDesc;

struct RHIViewportDesc
{
    float top_left_x;
    float top_left_y;
    float width;
    float height;
    float min_depth;
    float max_depth;
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

struct RHIDescriptorDesc
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

struct RHIUAVStructuredBufferDesc
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

struct RHIBufferDescriptorDesc : RHIDescriptorDesc
{
    RHIBufferDescriptorDesc(RHIDataFormat format, RHIViewType view_type, unsigned size, unsigned offset, const RHIUAVStructuredBufferDesc& uav_structured_desc)
            : RHIDescriptorDesc(format, RHIResourceDimension::BUFFER, view_type)
            , m_size(size)
            , m_offset(offset)
            , m_uav_structured_buffer_desc(uav_structured_desc)
    {
    }

    RHIBufferDescriptorDesc(RHIDataFormat format, RHIViewType view_type, unsigned size, unsigned offset, const RHISRVStructuredBufferDesc& srv_structured_desc)
            : RHIDescriptorDesc(format, RHIResourceDimension::BUFFER, view_type)
            , m_size(size)
            , m_offset(offset)
            , m_srv_structured_buffer_desc(srv_structured_desc)
    {
    }
    
    RHIBufferDescriptorDesc(RHIDataFormat format, RHIViewType view_type, unsigned size, unsigned offset)
        : RHIDescriptorDesc(format, RHIResourceDimension::BUFFER, view_type)
        , m_size(size)
        , m_offset(offset)
        , m_uav_structured_buffer_desc({})
    {
    }
    
    unsigned m_size;
    unsigned m_offset;
    
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

struct RHITextureDescriptorDesc : RHIDescriptorDesc
{
    RHITextureDescriptorDesc(RHIDataFormat format, RHIResourceDimension dimension, RHIViewType view_type)
        : RHIDescriptorDesc(format, dimension, view_type)
    {}
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
    s,
    Unknown,
};

struct RHIRootParameterDescriptorRangeDesc
{
    RHIRootParameterDescriptorRangeType type {RHIRootParameterDescriptorRangeType::Unknown} ;
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

struct RootSignatureAllocation
{
    RootSignatureAllocation()
        : parameter_name("")
        , global_parameter_index(0)
        , local_space_parameter_index(0)
        , register_index(0)
        , space(0)
        , type(RHIRootParameterType::Unknown)
        , register_type(RHIShaderRegisterType::Unknown)
    {
    }

    void AddShaderDefine(RHIShaderPreDefineMacros& out_shader_macros) const;

    std::string parameter_name;
    unsigned global_parameter_index;
    unsigned local_space_parameter_index;
    unsigned register_index;
    unsigned space;
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

struct RootSignatureStaticSamplerElement
{
    std::string sampler_name;
    unsigned register_space;
    unsigned sample_index;
    RHIStaticSamplerAddressMode address_mode;
    RHIStaticSamplerFilterMode filter_mode;
};

struct RootSignatureParameterElement
{
    std::string name;
    unsigned global_parameter_index;
    unsigned local_space_parameter_index;
    std::pair<unsigned, unsigned> register_range;
    unsigned space;
    unsigned constant_value_count;
    RHIRootParameterDescriptorRangeType table_type;
    bool is_bindless;
    bool is_buffer;
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
        glm::vec4 clear_color;
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
    RUF_NONE                =  0x0,
    RUF_ALLOW_CBV           =  0x1,
    RUF_ALLOW_UAV           =  0x1 << 1,
    RUF_ALLOW_SRV           =  0x1 << 2,
    
    // TEXTURE
    RUF_ALLOW_DEPTH_STENCIL =  0x1 << 3,
    RUF_ALLOW_RENDER_TARGET =  0x1 << 4,
    
    // BUFFER
    RUF_VERTEX_BUFFER       =  0x1 << 5,
    RUF_INDEX_BUFFER        =  0x1 << 6,
    RUF_INDIRECT_BUFFER     =  0x1 << 7,

    // COMMON
    RUF_TRANSFER_SRC        =  0x1 << 8,
    RUF_TRANSFER_DST        =  0x1 << 9,
};

struct RHITextureDesc
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

    RHIResourceUsageFlags GetUsage() const {return m_usage; }
    
    const RHITextureClearValue& GetClearValue() const;
    const std::string& GetName() const;
    void SetName(const std::string& name);

    static RHITextureDesc MakeFullScreenTextureDesc(const std::string& name, RHIDataFormat format, RHIResourceUsageFlags usage, const RHITextureClearValue& clear_value,const glTFRenderResourceManager& resource_manager);
    
    static RHITextureDesc MakeDepthTextureDesc(const glTFRenderResourceManager& resource_manager);
    static RHITextureDesc MakeScreenUVOffsetTextureDesc(const glTFRenderResourceManager& resource_manager);

    static RHITextureDesc MakeBasePassAlbedoTextureDesc(const glTFRenderResourceManager& resource_manager);
    static RHITextureDesc MakeBasePassNormalTextureDesc(const glTFRenderResourceManager& resource_manager);
    static RHITextureDesc MakeLightingPassOutputTextureDesc(const glTFRenderResourceManager& resource_manager);
    static RHITextureDesc MakeRayTracingSceneOutputTextureDesc(const glTFRenderResourceManager& resource_manager);
    static RHITextureDesc MakeRayTracingPassReSTIRSampleOutputDesc(const glTFRenderResourceManager& resource_manager);
    static RHITextureDesc MakeComputePassRayTracingPostProcessOutputDesc(const glTFRenderResourceManager& resource_manager);
    
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
    unsigned width;
    unsigned height;
};

struct RHIBeginRenderingInfo
{
    std::vector<IRHITextureDescriptorAllocation*> m_render_targets;
    unsigned rendering_area_width {0};
    unsigned rendering_area_height {0};
    bool enable_depth_write {false};
    bool clear_render_target {false};
};
