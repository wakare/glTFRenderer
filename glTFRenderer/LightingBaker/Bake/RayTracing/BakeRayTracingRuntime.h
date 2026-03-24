#pragma once

#include "BakeRayTracingScene.h"

#include <memory>
#include <string>
#include <vector>

#include "Renderer.h"
#include "RendererInterface.h"
#include "RHIInterface/IRHIRayTracingAS.h"
#include "RHIInterface/RHIIndexBuffer.h"
#include "RHIInterface/RHIVertexBuffer.h"

namespace LightingBaker
{
    struct BakeRayTracingRuntimeSettings
    {
        RendererInterface::RenderDeviceType device_type{RendererInterface::DX12};
        unsigned hidden_window_width{64u};
        unsigned hidden_window_height{64u};
        unsigned back_buffer_count{2u};
    };

    struct BakeRayTracingRuntimeBuildResult
    {
        RendererInterface::RenderDeviceType device_type{RendererInterface::DX12};
        unsigned hidden_window_width{0u};
        unsigned hidden_window_height{0u};
        unsigned frame_slot_count{0u};
        unsigned swapchain_image_count{0u};
        std::size_t uploaded_geometry_count{0u};
        std::size_t uploaded_instance_count{0u};
        std::size_t uploaded_vertex_count{0u};
        std::size_t uploaded_index_count{0u};
        std::size_t uploaded_shading_vertex_count{0u};
        std::size_t uploaded_shading_index_count{0u};
        std::size_t uploaded_shading_instance_count{0u};
        std::size_t uploaded_material_texture_count{0u};
        bool window_created{false};
        bool resource_operator_initialized{false};
        bool acceleration_structure_initialized{false};
        bool scene_vertex_buffer_created{false};
        bool scene_index_buffer_created{false};
        bool scene_instance_buffer_created{false};
        bool material_texture_table_created{false};
        std::shared_ptr<RendererInterface::RenderWindow> window{};
        std::shared_ptr<RendererInterface::ResourceOperator> resource_operator{};
        std::shared_ptr<IRHIRayTracingAS> acceleration_structure{};
        RendererInterface::BufferHandle scene_vertex_buffer_handle{};
        RendererInterface::BufferHandle scene_index_buffer_handle{};
        RendererInterface::BufferHandle scene_instance_buffer_handle{};
        std::vector<RendererInterface::TextureHandle> material_texture_handles{};
        std::vector<std::shared_ptr<RHIVertexBuffer>> vertex_buffers{};
        std::vector<std::shared_ptr<RHIIndexBuffer>> index_buffers{};
        std::vector<BakeSceneValidationMessage> errors{};
        std::vector<BakeSceneValidationMessage> warnings{};

        bool HasErrors() const;
        bool HasValidationErrors() const;
        void Shutdown();
    };

    class BakeRayTracingRuntimeBuilder
    {
    public:
        bool BuildRuntime(const BakeRayTracingSceneBuildResult& ray_tracing_scene,
                          const BakeRayTracingRuntimeSettings& settings,
                          BakeRayTracingRuntimeBuildResult& out_result,
                          std::wstring& out_error) const;
    };
}
