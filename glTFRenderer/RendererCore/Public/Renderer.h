#pragma once

#include <functional>
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
    typedef unsigned RenderGraphNodeHandle;
    typedef unsigned RenderSceneHandle;

    #define NULL_HANDLE UINT_MAX
    
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

    enum ResourceUsage
    {
        NONE            = 0,
        RENDER_TARGET   = 0x1,
        COPY_SRC        = 0x10,
        COPY_DST        = 0x100,
        SHADER_RESOURCE = 0x1000,
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
        ResourceUsage usage;
    };
    
    struct TextureDesc
    {
        PixelFormat format;
        unsigned width;
        unsigned height;
        std::vector<char> data;        
    };

    enum BufferType
    {
        UPLOAD,
        DEFAULT,
    };
    
    enum BufferUsage
    {
        USAGE_NONE,
        USAGE_VERTEX_BUFFER,
        USAGE_INDEX_BUFFER,
        USAGE_CBV,
        USAGE_UAV,
        USAGE_SRV,
    };

    struct BufferDesc
    {
        std::string name;
        BufferUsage usage;
        BufferType type;
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
        COLOR,
        DEPTH_STENCIL,
    };

    struct RenderTargetBindingDesc
    {
        PixelFormat format;
        RenderPassResourceUsage usage;
    };
    
    struct RenderPassDesc
    {
        RenderPassType type{};
        std::map<ShaderType, ShaderHandle> shaders;

        // resource desc
        std::vector<RenderTargetBindingDesc> render_target_bindings;
        
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

    enum class ExecuteCommandType
    {
        DRAW_VERTEX_COMMAND,
        DRAW_VERTEX_INSTANCING_COMMAND,
        DRAW_INDEXED_COMMAND,
        DRAW_INDEXED_INSTANCING_COMMAND,
        COMPUTE_DISPATCH_COMMAND,
        RAY_TRACING_COMMAND,
    };

    struct ExecuteCommandInputBuffer
    {
        // Vertex buffer is optional (fetch from huge buffer)
        BufferHandle vertex_buffer_handle;

        // Index buffer is optional (only necessary in DrawIndexed** command)
        BufferHandle index_buffer_handle;
    };

    struct DrawIndexedCommandParameter
    {
        unsigned vertex_count;
        unsigned index_count;
    };

    struct DrawVertexCommandParameter
    {
        unsigned vertex_count;
        unsigned start_vertex_location;
    };

    struct DrawVertexInstanceParameter
    {
        unsigned vertex_count;
        unsigned instance_count;
        unsigned start_vertex_location;
        unsigned start_instance_location;
    };
    
    struct ExecuteCommandParameter
    {
        union 
        {
            DrawIndexedCommandParameter     draw_indexed_command_parameter;
            DrawVertexCommandParameter      draw_vertex_command_parameter;
            DrawVertexInstanceParameter     draw_vertex_instance_command_parameter;
        };
    };
    
    struct RenderExecuteCommand
    {
        ExecuteCommandType type;
        ExecuteCommandInputBuffer input_buffer;
        ExecuteCommandParameter parameter;
    };

    struct BufferBindingDesc
    {
        enum BufferBindingType
        {
            CBV,
            SRV,
            UAV,
        };
        
        BufferHandle buffer_handle;
        BufferBindingType binding_type;

        // structured buffer descriptor config
        unsigned stride {0};
        unsigned count {0};
        bool is_structured_buffer {true};

        // only uav
        bool use_count_buffer {false};
        unsigned count_buffer_offset {0};
    };

    struct RenderPassDrawDesc
    {
        RenderExecuteCommand execute_command;
        std::map<RenderTargetHandle, RenderTargetBindingDesc> render_target_resources;
        std::map<RenderTargetHandle, bool> render_target_clear_states;
        std::map<std::string, BufferBindingDesc> buffer_resources;
    };

    struct RenderGraphNodeDesc
    {
        RenderPassHandle render_pass_handle;
        RenderPassDrawDesc draw_info;

        std::vector<RenderGraphNodeHandle> dependency_render_graph_nodes;

        std::function<void()> pre_render_callback;
    };

    struct RenderSceneDesc
    {
        std::string scene_file_name;
    };
}
