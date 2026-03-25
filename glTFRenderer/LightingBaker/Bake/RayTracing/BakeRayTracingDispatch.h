#pragma once

#include "Bake/Atlas/LightmapAtlasBuilder.h"
#include "Bake/RayTracing/BakeRayTracingRuntime.h"
#include "Scene/BakeSceneImporter.h"

#include "RendererInterface.h"

#include <filesystem>

namespace LightingBaker
{
    struct BakeRayTracingDispatchSettings
    {
        unsigned dispatch_width{0u};
        unsigned dispatch_height{0u};
        unsigned dispatch_depth{1u};
        unsigned sample_index{0u};
        unsigned sample_count{1u};
        unsigned max_bounces{1u};
    };

    struct BakeRayTracingDispatchRunResult
    {
        std::filesystem::path shader_path{};
        std::string acceleration_structure_binding_name{"g_scene_as"};
        std::string texel_record_binding_name{"g_bake_texel_records"};
        std::string scene_vertex_binding_name{"g_scene_vertices"};
        std::string scene_index_binding_name{"g_scene_indices"};
        std::string scene_instance_binding_name{"g_scene_instances"};
        std::string scene_light_binding_name{"g_scene_lights"};
        std::string material_texture_binding_name{"bindless_bake_material_textures"};
        std::string dispatch_constants_binding_name{"g_bake_dispatch_constants"};
        std::string output_binding_name{"g_bake_output"};
        std::string raygen_entry{"BakeRayGenMain"};
        std::string miss_entry{"BakeMissMain"};
        std::string closest_hit_entry{"BakeClosestHitMain"};
        std::string any_hit_entry{"BakeAnyHitMain"};
        std::string hit_group_export{"BakeHitGroup"};
        unsigned atlas_resolution{0u};
        unsigned texel_record_count{0u};
        unsigned dense_texel_record_count{0u};
        unsigned scene_light_count{0u};
        unsigned dispatch_width{0u};
        unsigned dispatch_height{0u};
        unsigned dispatch_depth{1u};
        unsigned sample_index{0u};
        unsigned sample_count{1u};
        unsigned max_bounces{1u};
        unsigned output_width{0u};
        unsigned output_height{0u};
        unsigned output_row_pitch{0u};
        unsigned output_nonzero_rgb_texel_count{0u};
        unsigned output_nonzero_alpha_texel_count{0u};
        unsigned output_trace_payload_sentinel_texel_count{0u};
        std::size_t output_readback_size{0u};
        bool shader_path_resolved{false};
        bool acceleration_structure_root_allocation_found{false};
        bool texel_record_root_allocation_found{false};
        bool scene_vertex_root_allocation_found{false};
        bool scene_index_root_allocation_found{false};
        bool scene_instance_root_allocation_found{false};
        bool scene_light_root_allocation_found{false};
        bool material_texture_root_allocation_found{false};
        bool dispatch_constants_root_allocation_found{false};
        bool output_root_allocation_found{false};
        bool texel_record_buffer_created{false};
        bool scene_vertex_buffer_bound{false};
        bool scene_index_buffer_bound{false};
        bool scene_instance_buffer_bound{false};
        bool scene_light_buffer_bound{false};
        bool material_texture_table_bound{false};
        bool dispatch_constants_buffer_created{false};
        bool output_render_target_created{false};
        bool render_pass_created{false};
        bool shader_table_initialized{false};
        bool pre_dispatch_uploads_flushed{false};
        bool graph_node_registered{false};
        bool compile_requested{false};
        bool event_loop_entered{false};
        bool close_requested{false};
        bool frame_executed{false};
        bool readback_buffer_allocated{false};
        bool readback_copy_recorded{false};
        bool readback_downloaded{false};
        bool output_payload_valid{false};
        RendererInterface::RenderGraph::FrameStats frame_stats{};
        RendererInterface::RenderWindow::WindowLoopTiming window_loop_timing{};
        std::vector<float> output_rgba{};
        std::vector<BakeSceneValidationMessage> errors{};
        std::vector<BakeSceneValidationMessage> warnings{};

        bool HasErrors() const;
        bool HasValidationErrors() const;
    };

    class BakeRayTracingDispatchRunner
    {
    public:
        bool RunDispatch(const LightmapAtlasBuildResult& atlas_result,
                         BakeRayTracingRuntimeBuildResult& runtime_result,
                         const BakeRayTracingDispatchSettings& settings,
                         BakeRayTracingDispatchRunResult& out_result,
                         std::wstring& out_error) const;
    };
}
