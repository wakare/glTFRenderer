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
        RGBA8,
        RGBA16,
    };

    struct RenderTargetDesc
    {
        PixelFormat format;
        unsigned width;
        unsigned height;
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

    enum RenderPassAccessMode
    {
        READ_ONLY,
        WRITE_ONLY,
        READ_WRITE,
    };
    
    struct RenderPassDesc
    {
        RenderPassType type{};
        std::map<ShaderType, ShaderHandle> shaders;

        // access modes
        std::map<TextureHandle, RenderPassAccessMode> texture_access_modes;
        std::map<RenderTargetHandle, RenderPassAccessMode> render_target_access_modes;
        std::map<BufferHandle, RenderPassAccessMode> buffer_access_modes;
        
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
