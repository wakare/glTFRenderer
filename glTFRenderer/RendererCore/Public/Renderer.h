#pragma once

#include <functional>
#include <limits>
#include <map>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include <Windows.h>

namespace RendererInterface
{
    struct NullHandle_t
    {
        explicit constexpr NullHandle_t() = default;
    };

    inline constexpr NullHandle_t NULL_HANDLE{};

    template <typename Tag>
    struct Handle
    {
        unsigned value{InvalidValue()};

        constexpr Handle() = default;
        constexpr Handle(NullHandle_t) : value(InvalidValue()) {}
        explicit constexpr Handle(unsigned v) : value(v) {}

        static constexpr unsigned InvalidValue() { return (std::numeric_limits<unsigned>::max)(); }
        static constexpr Handle Invalid() { return Handle(InvalidValue()); }
        constexpr bool IsValid() const { return value != InvalidValue(); }

        friend constexpr bool operator==(Handle lhs, Handle rhs) { return lhs.value == rhs.value; }
        friend constexpr bool operator!=(Handle lhs, Handle rhs) { return lhs.value != rhs.value; }
        friend constexpr bool operator<(Handle lhs, Handle rhs) { return lhs.value < rhs.value; }

        friend constexpr bool operator==(Handle lhs, NullHandle_t) { return !lhs.IsValid(); }
        friend constexpr bool operator!=(Handle lhs, NullHandle_t) { return lhs.IsValid(); }
        friend constexpr bool operator==(NullHandle_t, Handle rhs) { return !rhs.IsValid(); }
        friend constexpr bool operator!=(NullHandle_t, Handle rhs) { return rhs.IsValid(); }
    };

    struct ShaderHandleTag {};
    struct TextureHandleTag {};
    struct BufferHandleTag {};
    struct IndexedBufferHandleTag {};
    struct RenderTargetHandleTag {};
    struct RenderDeviceHandleTag {};
    struct RenderPassHandleTag {};
    struct RenderWindowHandleTag {};
    struct RenderGraphNodeHandleTag {};
    struct RenderSceneHandleTag {};

    // resource type handle define
    using ShaderHandle = Handle<ShaderHandleTag>;
    using TextureHandle = Handle<TextureHandleTag>;
    using BufferHandle = Handle<BufferHandleTag>;
    using IndexedBufferHandle = Handle<IndexedBufferHandleTag>;
    using RenderTargetHandle = Handle<RenderTargetHandleTag>;
    
    using RenderDeviceHandle = Handle<RenderDeviceHandleTag>;
    using RenderPassHandle = Handle<RenderPassHandleTag>;
    using RenderWindowHandle = Handle<RenderWindowHandleTag>;
    using RenderGraphNodeHandle = Handle<RenderGraphNodeHandleTag>;
    using RenderSceneHandle = Handle<RenderSceneHandleTag>;
    
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
        RGBA16_FLOAT,
        R32_FLOAT,
        R16_FLOAT,
        R8_UNORM,
        D32,
    };

    enum ResourceUsage
    {
        NONE            = 0,
        RENDER_TARGET   = 0x1,
        COPY_SRC        = 0x10,
        COPY_DST        = 0x100,
        SHADER_RESOURCE = 0x1000,
        DEPTH_STENCIL   = 0x10000,
        UNORDER_ACCESS  = 0x100000,
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
    static RenderTargetClearValue default_clear_depth = { .clear_depth_stencil = {1.0f, 0} };

    enum class RenderTargetSizeMode
    {
        FIXED,
        WINDOW_RELATIVE,
    };
    
    struct RenderTargetDesc
    {
        std::string name;
        PixelFormat format;
        unsigned width;
        unsigned height;
        RenderTargetClearValue clear;
        ResourceUsage usage;
        bool enable_mipmaps{false};
        RenderTargetSizeMode size_mode{RenderTargetSizeMode::FIXED};
        float width_scale{1.0f};
        float height_scale{1.0f};
        unsigned min_width{1};
        unsigned min_height{1};
    };
    
    struct TextureDesc
    {
        PixelFormat format;
        unsigned width;
        unsigned height;
        std::vector<char> data;
        bool generate_mipmaps{false};
    };

    struct TextureFileDesc
    {
        std::string uri;
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
        USAGE_INDEX_BUFFER_R32,
        USAGE_INDEX_BUFFER_R16,
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
        std::optional<void*> data;
    };

    enum class RenderPassType
    {
        GRAPHICS,
        COMPUTE,
        RAY_TRACING,
    };

    enum class RenderPassResourceAccessMode
    {
        READ_ONLY,
        WRITE_ONLY,
        READ_WRITE,
    };

    enum class RenderPassResourceUsage
    {
        COLOR,
        DEPTH_STENCIL,
    };

    enum class RenderPassAttachmentLoadOp
    {
        LOAD,
        CLEAR,
        DONT_CARE,
    };

    enum class RenderPassAttachmentStoreOp
    {
        STORE,
        DONT_CARE,
    };

    enum class CullMode
    {
        NONE,
        CW,
        CCW,
    };

    enum class DepthStencilMode
    {
        DEPTH_READ,
        DEPTH_WRITE,
        DEPTH_DONT_CARE,
    };

    enum class PrimitiveTopology
    {
        TRIANGLE_LIST,
    };

    enum class BlendMode
    {
        BLEND_OPAQUE,
        BLEND_ALPHA,
        BLEND_ADDITIVE,
    };

    struct RenderStateDesc
    {
        CullMode cull_mode{CullMode::NONE};
        DepthStencilMode depth_stencil_mode{DepthStencilMode::DEPTH_WRITE};
        PrimitiveTopology primitive_topology{PrimitiveTopology::TRIANGLE_LIST};
        BlendMode blend_mode{BlendMode::BLEND_OPAQUE};
    };

    struct RenderTargetBindingDesc
    {
        PixelFormat format;
        RenderPassResourceUsage usage;
        bool need_clear;
        RenderPassAttachmentLoadOp load_op{RenderPassAttachmentLoadOp::LOAD};
        RenderPassAttachmentStoreOp store_op{RenderPassAttachmentStoreOp::STORE};
    };
    
    struct RenderPassDesc
    {
        RenderPassType type{};
        std::map<ShaderType, ShaderHandle> shaders;
        RenderStateDesc render_state{};

        // resource desc
        std::vector<RenderTargetBindingDesc> render_target_bindings;

        // viewport
        int viewport_width{-1};
        int viewport_height{-1};
        
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

    // Runtime state for swapchain/surface synchronization.
    enum class SwapchainLifecycleState
    {
        UNINITIALIZED,
        READY,
        RESIZE_PENDING,
        RESIZE_DEFERRED,
        RECOVERING,
        MINIMIZED,
        INVALID,
    };

    enum class WindowSurfaceSyncStatus
    {
        READY,
        RESIZED,
        DEFERRED_RETRY,
        MINIMIZED,
        INVALID,
    };

    struct WindowSurfaceSyncResult
    {
        WindowSurfaceSyncStatus status{WindowSurfaceSyncStatus::INVALID};
        SwapchainLifecycleState lifecycle_state{SwapchainLifecycleState::UNINITIALIZED};
        unsigned window_width{0};
        unsigned window_height{0};
        unsigned render_width{0};
        unsigned render_height{0};
        bool swapchain_resized{false};
        bool window_render_targets_resized{false};
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
        IndexedBufferHandle index_buffer_handle;
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

    struct DrawIndexedInstanceParameter
    {
        unsigned index_count_per_instance;
        unsigned instance_count;
        unsigned start_index_location;
        unsigned start_vertex_location;
        unsigned start_instance_location;
    };

    struct DispatchParameter
    {
        unsigned group_size_x;
        unsigned group_size_y;
        unsigned group_size_z;
    };
    
    struct ExecuteCommandParameter
    {
        union 
        {
            DrawIndexedCommandParameter     draw_indexed_command_parameter;
            DrawVertexCommandParameter      draw_vertex_command_parameter;
            DrawVertexInstanceParameter     draw_vertex_instance_command_parameter;
            DrawIndexedInstanceParameter    draw_indexed_instance_command_parameter;
            DispatchParameter               dispatch_parameter;
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

    struct TextureBindingDesc
    {
        enum TextureBindingType
        {
            SRV,
            UAV,
        };

        std::vector<TextureHandle> textures;
        
        TextureBindingType type;
    };

    struct RenderTargetTextureBindingDesc
    {
        enum TextureBindingType
        {
            SRV,
            UAV,
        };

        std::string name;
        std::vector<RenderTargetHandle> render_target_texture;
        TextureBindingType type;
    };

    struct RenderPassDrawDesc
    {
        std::vector<RenderExecuteCommand> execute_commands;
        std::map<RenderTargetHandle, RenderTargetBindingDesc> render_target_resources;
        std::map<std::string, BufferBindingDesc> buffer_resources;
        std::map<std::string, TextureBindingDesc> texture_resources;
        std::map<std::string, RenderTargetTextureBindingDesc> render_target_texture_resources;
    };

    struct RenderGraphNodeDesc
    {
        RenderPassHandle render_pass_handle;
        RenderPassDrawDesc draw_info;

        std::vector<RenderGraphNodeHandle> dependency_render_graph_nodes;

        std::function<void(unsigned long long)> pre_render_callback;
        std::string debug_group;
        std::string debug_name;
    };

    struct RenderSceneDesc
    {
        std::string scene_file_name;
    };
}

using RendererInterface::NULL_HANDLE;
