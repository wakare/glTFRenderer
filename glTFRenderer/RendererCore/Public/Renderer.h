#pragma once

#include <map>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include <Windows.h>

namespace RendererInterface
{
     // resource type handle define
    typedef unsigned ShaderHandle;
    typedef unsigned TextureHandle;
    typedef unsigned BufferHandle;
    typedef unsigned RenderTargetHandle;
    
    typedef unsigned RenderDeviceHandle;
    typedef unsigned RenderPassHandle;
    typedef unsigned RenderWindowHandle;
    
    enum ShaderType
    {
        VERTEX_SHADER = 0,
        FRAGMENT_SHADER = 1,
        COMPUTE_SHADER = 2,
        RAY_TRACING_SHADER = 3,
    };
    
    struct ShaderDesc
    {
        ShaderType shader_type;
        std::string shader_file_name;
        std::string entry_point;
    };

    enum PixelFormat
    {
        RGBA8_UNORM,
        RGBA16_UNORM,
    };

    struct RenderTargetClearValue
    {
        union 
        {
            float clear_color[4];

            struct my_struct
            {
                float clear_depth;
                unsigned char clear_stencil;
            } clear_depth_stencil;
        };
    };

    static RenderTargetClearValue default_clear_color = { .clear_color{0.0f, 0.0f, 0.0f, 1.0f} };
    static RenderTargetClearValue default_clear_depth = { .clear_depth_stencil = {0.0f, 0} };
    
    struct RenderTargetDesc
    {
        std::string name;
        PixelFormat format;
        unsigned width;
        unsigned height;
        RenderTargetClearValue clear;
    };
    
    struct TextureDesc
    {
        PixelFormat format;
        unsigned width;
        unsigned height;
        std::vector<char> data;        
    };

    enum BufferUsage
    {
        VERTEX_BUFFER,
        INDEX_BUFFER,
    };

    struct BufferDesc
    {
        BufferUsage usage;
        size_t size;
        
        // optional
        std::optional<std::unique_ptr<char[]>> data;
    };

    enum RenderPassType
    {
        GRAPHICS,
        COMPUTE,
        RAY_TRACING,
    };

    enum RenderPassResourceAccessMode
    {
        READ_ONLY,
        WRITE_ONLY,
        READ_WRITE,
    };

    enum RenderPassResourceUsage
    {
        COLOR_INPUT,
        COLOR_OUTPUT,
        DEPTH_STENCIL_INPUT,
        DEPTH_STENCIL_OUTPUT,
    };

    struct RenderPassResourceDesc
    {
        RenderPassResourceUsage usage;
        RenderPassResourceAccessMode access_mode;
    };
    
    struct RenderPassDesc
    {
        RenderPassType type{};
        std::map<ShaderType, ShaderHandle> shaders;

        // resource desc
        std::map<TextureHandle, RenderPassResourceDesc> texture_resources;
        std::map<RenderTargetHandle, RenderPassResourceDesc> render_target_resources;
        std::map<BufferHandle, RenderPassResourceDesc> buffer_resources;
        
        // dependency
        std::vector<RenderPassHandle> dependency_render_passes;
    };

    enum RenderDeviceType
    {
        DX12,
        VULKAN,
    };
    
    struct RenderDeviceDesc
    {
        RenderDeviceType type;
        RenderWindowHandle window;
        unsigned back_buffer_count;
    };

    struct RenderWindowDesc
    {
        unsigned width;
        unsigned height;
    };
}
