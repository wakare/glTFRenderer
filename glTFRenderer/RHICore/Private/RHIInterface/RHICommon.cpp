#include "RHICommon.h"

#include "RHIInterface/IRHISwapChain.h"
#include "RHIConfigSingleton.h"
#include "RHIUtils.h"
#include "SceneFileLoader/glTFImageIOUtil.h"

void RootSignatureAllocation::AddShaderDefine(RHIShaderPreDefineMacros& out_shader_macros) const
{
    char shader_resource_declaration[64] = {'\0'};

    std::string register_name;
    switch (register_type) {
    case RHIShaderRegisterType::b:
        register_name = "b";
        break;
    case RHIShaderRegisterType::t:
        register_name = "t";
        break;
    case RHIShaderRegisterType::u:
        register_name = "u";
        break;
    case RHIShaderRegisterType::s:
        register_name = "s";
        break;
    case RHIShaderRegisterType::Unknown:
        GLTF_CHECK(false);
        break;
    }

    memset(shader_resource_declaration, 0, sizeof(shader_resource_declaration));
    if (RHIConfigSingleton::Instance().GetGraphicsAPIType() == RHIGraphicsAPIType::RHI_GRAPHICS_API_DX12)
    {
        (void)snprintf(shader_resource_declaration, sizeof(shader_resource_declaration), "register(%s%d, space%u)", register_name.c_str(), register_begin_index, space);    
    }
    else
    {
        (void)snprintf(shader_resource_declaration, sizeof(shader_resource_declaration), "[[vk::binding(%d, %d)]]", local_space_parameter_index, space);   
    }
    
    out_shader_macros.AddMacro(parameter_name, shader_resource_declaration);
}

RHITextureDesc::RHITextureDesc(const RHITextureDesc& desc) noexcept
{
    InitWithoutCopyData(desc);
}

RHITextureDesc& RHITextureDesc::operator=(const RHITextureDesc& desc) noexcept
{
    InitWithoutCopyData(desc);
    return *this;
}

RHITextureDesc::RHITextureDesc(RHITextureDesc&& desc) noexcept
{
    m_texture_data = std::move(desc.m_texture_data);
    m_texture_data_size = desc.m_texture_data_size;
    m_texture_width = desc.m_texture_width;
    m_texture_height = desc.m_texture_height;
    m_texture_format = desc.m_texture_format;
    m_usage = desc.m_usage;
    m_clear_value = desc.m_clear_value;
    m_name = desc.m_name;
}

RHITextureDesc& RHITextureDesc::operator=(RHITextureDesc&& desc) noexcept
{
    m_texture_data = std::move(desc.m_texture_data);
    m_texture_data_size = desc.m_texture_data_size;
    m_texture_width = desc.m_texture_width;
    m_texture_height = desc.m_texture_height;
    m_texture_format = desc.m_texture_format;
    m_usage = desc.m_usage;
    m_clear_value = desc.m_clear_value;
    m_name = desc.m_name;
    return *this;
}

RHITextureDesc::RHITextureDesc(
        std::string name,
        unsigned width,
        unsigned height,
        RHIDataFormat format,
        RHIResourceUsageFlags usage,
        const RHITextureClearValue& clear_value
    )
        : m_name(std::move(name))
        , m_texture_width(width)
        , m_texture_height(height)
        , m_texture_format(format)
        , m_usage(usage)
        , m_clear_value(clear_value)
{
    
}

bool RHITextureDesc::InitWithLoadedData(const ImageLoadResult& image_load_result)
{
    assert(m_texture_data == nullptr);

    m_texture_width = image_load_result.width;
    m_texture_height = image_load_result.height;
    m_texture_format = ConvertToRHIDataFormat(image_load_result.format);

    m_texture_data_size = image_load_result.data.size();
    m_texture_data.reset(static_cast<unsigned char*>(malloc(m_texture_data_size))) ;
    RETURN_IF_FALSE(m_texture_data)

    memcpy(m_texture_data.get(), image_load_result.data.data(), m_texture_data_size);
    m_usage = static_cast<RHIResourceUsageFlags>(RHIResourceUsageFlags::RUF_TRANSFER_DST | RHIResourceUsageFlags::RUF_ALLOW_SRV) ;

    m_clear_value.clear_format = m_texture_format;
    
    return true;
}

bool RHITextureDesc::InitWithoutCopyData(const RHITextureDesc& other)
{
    //m_texture_data = std::move(other.m_texture_data);
    m_texture_data_size = other.m_texture_data_size;
    m_texture_width = other.m_texture_width;
    m_texture_height = other.m_texture_height;
    m_texture_format = other.m_texture_format;
    m_usage = other.m_usage;
    m_clear_value = other.m_clear_value;
    m_name = other.m_name;
    
    return true;
}

bool RHITextureDesc::SetTextureData(const char* data, size_t byte_size)
{
    GLTF_CHECK(!HasTextureData());

    m_texture_data = std::make_unique<unsigned char[]>(byte_size);
    m_texture_data_size = byte_size;
    memcpy(m_texture_data.get(), data, byte_size);

    return true;
}

RHIDataFormat RHITextureDesc::ConvertToRHIDataFormat(const WICPixelFormatGUID& wicFormatGUID)
{
    if (wicFormatGUID == GUID_WICPixelFormat128bppRGBAFloat) return RHIDataFormat::R32G32B32A32_FLOAT;
    else if (wicFormatGUID == GUID_WICPixelFormat64bppRGBAHalf) return RHIDataFormat::R16G16B16A16_FLOAT;
    else if (wicFormatGUID == GUID_WICPixelFormat64bppRGBA) return RHIDataFormat::R16G16B16A16_UNORM;
    else if (wicFormatGUID == GUID_WICPixelFormat32bppRGBA) return RHIDataFormat::R8G8B8A8_UNORM;
    else if (wicFormatGUID == GUID_WICPixelFormat32bppBGRA) return RHIDataFormat::B8G8R8A8_UNORM;
    else if (wicFormatGUID == GUID_WICPixelFormat32bppBGR) return RHIDataFormat::B8G8R8X8_UNORM;
    else if (wicFormatGUID == GUID_WICPixelFormat32bppRGBA1010102XR) return RHIDataFormat::R10G10B10_XR_BIAS_A2_UNORM;

    else if (wicFormatGUID == GUID_WICPixelFormat32bppRGBA1010102) return RHIDataFormat::R10G10B10A2_UNORM;
    else if (wicFormatGUID == GUID_WICPixelFormat16bppBGRA5551) return RHIDataFormat::B5G5R5A1_UNORM;
    else if (wicFormatGUID == GUID_WICPixelFormat16bppBGR565) return RHIDataFormat::B5G6R5_UNORM;
    else if (wicFormatGUID == GUID_WICPixelFormat32bppGrayFloat) return RHIDataFormat::R32_FLOAT;
    else if (wicFormatGUID == GUID_WICPixelFormat16bppGrayHalf) return RHIDataFormat::R16_FLOAT;
    else if (wicFormatGUID == GUID_WICPixelFormat16bppGray) return RHIDataFormat::R16_UNORM;
    else if (wicFormatGUID == GUID_WICPixelFormat8bppGray) return RHIDataFormat::R8_UNORM;
    else if (wicFormatGUID == GUID_WICPixelFormat8bppAlpha) return RHIDataFormat::A8_UNORM;

    else return RHIDataFormat::UNKNOWN;
}

bool RHITextureDesc::HasUsage(RHIResourceUsageFlags usage) const
{
    return m_usage & usage;
}

const RHITextureClearValue& RHITextureDesc::GetClearValue() const
{
    return m_clear_value;
}

const std::string& RHITextureDesc::GetName() const
{
    return m_name;
}

void RHITextureDesc::SetName(const std::string& name)
{
    m_name = name;
}

RHITextureDesc RHITextureDesc::MakeFullScreenTextureDesc(const std::string& name, RHIDataFormat format,
                                                         RHIResourceUsageFlags usage, const RHITextureClearValue& clear_value, unsigned width, unsigned height)
{
    return RHITextureDesc
    {
        name,
        width,
        height,
        format,
        usage,
        clear_value
    };
}

RHITextureDesc RHITextureDesc::MakeDepthTextureDesc(unsigned width, unsigned height)
{
    RHITextureDesc texture_desc = MakeFullScreenTextureDesc(
        "DEPTH_TEXTURE_OUTPUT",
        RHIDataFormat::R32_TYPELESS,
        static_cast<RHIResourceUsageFlags>(RUF_ALLOW_DEPTH_STENCIL | RUF_ALLOW_SRV),
        {
            .clear_format = RHIDataFormat::D32_FLOAT,
            .clear_depth_stencil{1.0f, 0}
        },
        width, height
    );

    return texture_desc;
}

RHITextureDesc RHITextureDesc::MakeScreenUVOffsetTextureDesc(unsigned width, unsigned height)
{
    RHITextureDesc texture_desc = MakeFullScreenTextureDesc(
        "SCREEN_UV_OFFSET_OUTPUT",
        RHIDataFormat::R32G32B32A32_FLOAT,
        static_cast<RHIResourceUsageFlags>(RUF_ALLOW_UAV | RUF_ALLOW_RENDER_TARGET),
        {
            .clear_format = RHIDataFormat::R32G32B32A32_FLOAT,
            .clear_color {0.0f, 0.0f, 0.0f, 0.0f}
        },
        width, height
    );

    return texture_desc;
}

RHITextureDesc RHITextureDesc::MakeBasePassAlbedoTextureDesc(unsigned width, unsigned height)
{
    RHITextureDesc texture_desc = MakeFullScreenTextureDesc(
        "BASEPASS_ALBEDO_OUTPUT",
        RHIDataFormat::R8G8B8A8_UNORM,
        static_cast<RHIResourceUsageFlags>(RUF_ALLOW_SRV | RUF_ALLOW_UAV | RUF_ALLOW_RENDER_TARGET | RUF_TRANSFER_SRC ),
        {
            .clear_format = RHIDataFormat::R8G8B8A8_UNORM,
            .clear_color {0.0f, 0.0f, 0.0f, 0.0f}
        },
        width, height
    );

    return texture_desc;
}

RHITextureDesc RHITextureDesc::MakeBasePassNormalTextureDesc(unsigned width, unsigned height)
{
    RHITextureDesc texture_desc = MakeFullScreenTextureDesc(
        "BASEPASS_NORMAL_OUTPUT",
        RHIDataFormat::R8G8B8A8_UNORM,
        static_cast<RHIResourceUsageFlags>(RUF_ALLOW_SRV | RUF_ALLOW_UAV | RUF_ALLOW_RENDER_TARGET | RUF_TRANSFER_SRC),
        {
            .clear_format = RHIDataFormat::R8G8B8A8_UNORM,
            .clear_color {0.0f, 0.0f, 0.0f, 0.0f}
        },
        width, height
    );

    return texture_desc;
}

RHITextureDesc RHITextureDesc::MakeLightingPassOutputTextureDesc(
    unsigned width, unsigned height)
{
    RHITextureDesc texture_desc = MakeFullScreenTextureDesc(
        "LIGHTING_OUTPUT",
        RHIDataFormat::R8G8B8A8_UNORM,
        static_cast<RHIResourceUsageFlags>(RUF_ALLOW_UAV | RUF_TRANSFER_SRC),
        {
            .clear_format = RHIDataFormat::R8G8B8A8_UNORM,
            .clear_color {0.0f, 0.0f, 0.0f, 0.0f}
        },
        width, height
    );

    return texture_desc;
}

RHITextureDesc RHITextureDesc::MakeRayTracingSceneOutputTextureDesc(
    unsigned width, unsigned height)
{
    RHITextureDesc texture_desc = MakeFullScreenTextureDesc(
        "RAYTRACING_OUTPUT",
        RHIDataFormat::R8G8B8A8_UNORM,
        static_cast<RHIResourceUsageFlags>(RUF_ALLOW_UAV | RUF_ALLOW_RENDER_TARGET),
        {
            .clear_format = RHIDataFormat::R8G8B8A8_UNORM,
            .clear_color {0.0f, 0.0f, 0.0f, 0.0f}
        },
        width, height
    );

    return texture_desc;
}

RHITextureDesc RHITextureDesc::MakeRayTracingPassReSTIRSampleOutputDesc(
    unsigned width, unsigned height)
{
    RHITextureDesc texture_desc = MakeFullScreenTextureDesc(
        "RESTIR_DI_SAMPLE_OUTPUT",
        RHIDataFormat::R32G32B32A32_FLOAT,
        static_cast<RHIResourceUsageFlags>(RUF_ALLOW_UAV | RUF_ALLOW_RENDER_TARGET),
        {
            .clear_format = RHIDataFormat::R32G32B32A32_FLOAT,
            .clear_color {0.0f, 0.0f, 0.0f, 0.0f}
        },
        width, height
    );

    return texture_desc;
}

RHITextureDesc RHITextureDesc::MakeComputePassRayTracingPostProcessOutputDesc(
    unsigned width, unsigned height)
{
    RHITextureDesc texture_desc = MakeFullScreenTextureDesc(
        "POSTPROCESS_OUTPUT",
        RHIDataFormat::R8G8B8A8_UNORM,
        static_cast<RHIResourceUsageFlags>(RUF_ALLOW_UAV | RUF_ALLOW_RENDER_TARGET),
        {
            .clear_format = RHIDataFormat::R8G8B8A8_UNORM,
            .clear_color {0.0f, 0.0f, 0.0f, 0.0f}
        },
        width, height
    );

    return texture_desc;
}

RHITextureDesc RHITextureDesc::MakeShadowPassOutputDesc(unsigned width, unsigned height)
{
    RHITextureDesc texture_desc = MakeFullScreenTextureDesc(
        "SHADOWPASS_OUTPUT",
        RHIDataFormat::R32_TYPELESS,
        static_cast<RHIResourceUsageFlags>(RUF_ALLOW_DEPTH_STENCIL | RUF_ALLOW_SRV),
        {
            .clear_format = RHIDataFormat::D32_FLOAT,
            .clear_depth_stencil{1.0f, 0}
        },
        width, height
    );

    texture_desc.m_texture_width = width;
    texture_desc.m_texture_height = height;
    
    return texture_desc;
}

RHITextureDesc RHITextureDesc::MakeVirtualTextureFeedbackDesc(unsigned width, unsigned height, unsigned feed_back_size_x, unsigned feed_back_size_y)
{
    RHITextureDesc texture_desc = MakeFullScreenTextureDesc(
        "VT_FEEDBACK_OUTPUT",
        RHIDataFormat::R32G32B32A32_UINT,
        static_cast<RHIResourceUsageFlags>( RUF_ALLOW_SRV | RUF_ALLOW_RENDER_TARGET | RUF_TRANSFER_SRC),
        {
            .clear_format = RHIDataFormat::R32G32B32A32_UINT,
            .clear_color {0.0f, 0.0f, 0.0f, 0.0f}
        },
        width, height
    );
    texture_desc.m_texture_width = feed_back_size_x;
    texture_desc.m_texture_height = feed_back_size_y;

    return texture_desc;
}


unsigned IndexBufferData::GetStride() const
{
    if (format == RHIDataFormat::R16_UINT)
    {
        return sizeof(USHORT);
    }
    else if (format == RHIDataFormat::R32_UINT)
    {
        return sizeof(UINT);
        
    }
    GLTF_CHECK(false);
    
    return 0; 
}

unsigned IndexBufferData::GetIndexByOffset(size_t offset) const
{
    char* index_data = data.get() + GetStride() * offset;
    if (format == RHIDataFormat::R16_UINT)
    {
        const USHORT* index_data_ushort = reinterpret_cast<USHORT*>(index_data);
        return *index_data_ushort;
    }
    else if (format == RHIDataFormat::R32_UINT)
    {
        const UINT* index_data_uint = reinterpret_cast<UINT*>(index_data);
        return *index_data_uint;
    }
    
    GLTF_CHECK(false);
    return 0;
}