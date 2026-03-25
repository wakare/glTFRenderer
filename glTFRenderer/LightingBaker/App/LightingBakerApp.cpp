#include "LightingBakerApp.h"

#include "Bake/Atlas/LightmapAtlasBuilder.h"
#include "Bake/Post/BakeAccumulator.h"
#include "Bake/RayTracing/BakeRayTracingDispatch.h"
#include "Bake/RayTracing/BakeRayTracingRuntime.h"
#include "Bake/RayTracing/BakeRayTracingScene.h"
#include "Output/BakeOutputWriter.h"
#include "RendererCommon.h"
#include "Scene/BakeSceneImporter.h"

#include <iostream>

namespace LightingBaker
{
    namespace
    {
        constexpr int kExitSuccess = 0;
        constexpr int kExitInvalidArguments = 1;
        constexpr int kExitFilesystemError = 2;
        constexpr int kExitOutputWriteError = 3;
        constexpr int kExitSceneImportError = 4;
        constexpr int kExitSceneValidationError = 5;
        constexpr int kExitAtlasBuildError = 6;
        constexpr int kExitAtlasValidationError = 7;
        constexpr int kExitResumeValidationError = 8;
        constexpr int kExitAccumulationError = 9;
        constexpr int kExitRayTracingSceneError = 10;
        constexpr int kExitRayTracingRuntimeError = 11;
        constexpr int kExitRayTracingDispatchError = 12;

        unsigned CountErrors(const BakeSceneImportResult& import_result)
        {
            unsigned count = static_cast<unsigned>(import_result.errors.size());
            for (const BakePrimitiveImportInfo& primitive : import_result.primitive_instances)
            {
                count += static_cast<unsigned>(primitive.errors.size());
            }

            return count;
        }

        unsigned CountWarnings(const BakeSceneImportResult& import_result)
        {
            unsigned count = static_cast<unsigned>(import_result.warnings.size());
            for (const BakePrimitiveImportInfo& primitive : import_result.primitive_instances)
            {
                count += static_cast<unsigned>(primitive.warnings.size());
            }

            return count;
        }

        unsigned CountErrors(const LightmapAtlasBuildResult& atlas_result)
        {
            return static_cast<unsigned>(atlas_result.errors.size());
        }

        unsigned CountWarnings(const LightmapAtlasBuildResult& atlas_result)
        {
            return static_cast<unsigned>(atlas_result.warnings.size());
        }

        unsigned CountErrors(const BakeRayTracingSceneBuildResult& ray_tracing_scene)
        {
            return static_cast<unsigned>(ray_tracing_scene.errors.size());
        }

        unsigned CountWarnings(const BakeRayTracingSceneBuildResult& ray_tracing_scene)
        {
            return static_cast<unsigned>(ray_tracing_scene.warnings.size());
        }

        unsigned CountErrors(const BakeRayTracingRuntimeBuildResult& ray_tracing_runtime)
        {
            return static_cast<unsigned>(ray_tracing_runtime.errors.size());
        }

        unsigned CountWarnings(const BakeRayTracingRuntimeBuildResult& ray_tracing_runtime)
        {
            return static_cast<unsigned>(ray_tracing_runtime.warnings.size());
        }

        unsigned CountErrors(const BakeRayTracingDispatchRunResult& ray_tracing_dispatch)
        {
            return static_cast<unsigned>(ray_tracing_dispatch.errors.size());
        }

        unsigned CountWarnings(const BakeRayTracingDispatchRunResult& ray_tracing_dispatch)
        {
            return static_cast<unsigned>(ray_tracing_dispatch.warnings.size());
        }
    }

    int LightingBakerApp::Run(int argc, wchar_t* argv[])
    {
        const CommandLineParseResult parse_result = ParseCommandLine(argc, argv);
        if (!parse_result.error_message.empty())
        {
            std::wcerr << parse_result.error_message << L"\n\n" << BuildUsageText();
            return kExitInvalidArguments;
        }

        if (parse_result.config.show_help)
        {
            std::wcout << BuildUsageText();
            return kExitSuccess;
        }

        return RunJob(parse_result.config);
    }

    int LightingBakerApp::RunJob(const BakeJobConfig& config)
    {
        BakeOutputWriter output_writer{};
        const BakeOutputLayout output_layout = BakeOutputWriter::BuildLayout(config.output_root);
        BakeAccumulator accumulator{};
        BakeAccumulatorResumeState resume_state{};

        std::wstring error_message;
        if (!output_writer.EnsureLayout(output_layout, error_message))
        {
            std::wcerr << error_message << L"\n";
            return kExitFilesystemError;
        }

        if (config.resume)
        {
            error_message.clear();
            if (!accumulator.LoadResumeState(output_layout.resume_path, resume_state, error_message))
            {
                std::wcerr << error_message << L"\n";
                return kExitResumeValidationError;
            }
        }

        BakeSceneImporter scene_importer{};
        BakeSceneImportRequest scene_import_request{};
        scene_import_request.scene_path = config.scene_path;
        scene_import_request.material_texture_cache_root = output_layout.cache / L"material_textures";
        BakeSceneImportResult import_result{};
        const bool import_success = scene_importer.ImportScene(scene_import_request, import_result, error_message);
        const std::wstring import_error_message = error_message;

        error_message.clear();
        if (!output_writer.WriteImportSummary(import_result, output_layout, error_message))
        {
            std::wcerr << error_message << L"\n";
            return kExitOutputWriteError;
        }

        error_message.clear();
        if (!import_success)
        {
            if (!config.resume &&
                !output_writer.WriteBootstrapPackage(config, output_layout, import_result, nullptr, error_message))
            {
                std::wcerr << error_message << L"\n";
                return kExitOutputWriteError;
            }

            PrintResolvedJob(config, output_layout);
            PrintImportSummary(import_result);
            std::wcerr << import_error_message << L"\n";
            return kExitSceneImportError;
        }

        if (import_result.HasValidationErrors())
        {
            if (!config.resume &&
                !output_writer.WriteBootstrapPackage(config, output_layout, import_result, nullptr, error_message))
            {
                std::wcerr << error_message << L"\n";
                return kExitOutputWriteError;
            }

            PrintResolvedJob(config, output_layout);
            PrintImportSummary(import_result);
            std::wcerr << L"Scene validation failed. See import summary for details.\n";
            return kExitSceneValidationError;
        }

        LightmapAtlasBuilder atlas_builder{};
        LightmapAtlasBuildSettings atlas_settings{};
        atlas_settings.atlas_resolution = config.atlas_resolution;
        LightmapAtlasBuildResult atlas_result{};
        error_message.clear();
        if (!atlas_builder.BuildAtlas(import_result, atlas_settings, atlas_result, error_message))
        {
            std::wcerr << error_message << L"\n";
            return kExitAtlasBuildError;
        }

        error_message.clear();
        if (!output_writer.WriteAtlasSummary(atlas_result, output_layout, error_message))
        {
            std::wcerr << error_message << L"\n";
            return kExitOutputWriteError;
        }

        BakeRayTracingSceneBuilder ray_tracing_scene_builder{};
        BakeRayTracingSceneBuildResult ray_tracing_scene{};
        error_message.clear();
        if (!ray_tracing_scene_builder.BuildScene(import_result, ray_tracing_scene, error_message))
        {
            std::wcerr << error_message << L"\n";
            return kExitRayTracingSceneError;
        }

        error_message.clear();
        if (!output_writer.WriteRayTracingSceneSummary(ray_tracing_scene, output_layout, error_message))
        {
            std::wcerr << error_message << L"\n";
            return kExitOutputWriteError;
        }

        if (ray_tracing_scene.HasValidationErrors())
        {
            PrintResolvedJob(config, output_layout);
            PrintImportSummary(import_result);
            PrintAtlasSummary(atlas_result);
            PrintRayTracingSceneSummary(ray_tracing_scene);
            std::wcerr << L"Ray tracing scene validation failed. See ray tracing scene summary for details.\n";
            return kExitRayTracingSceneError;
        }

        if (atlas_result.HasValidationErrors())
        {
            if (!config.resume)
            {
                error_message.clear();
                if (!output_writer.WriteBootstrapPackage(config, output_layout, import_result, &atlas_result, error_message))
                {
                    std::wcerr << error_message << L"\n";
                    return kExitOutputWriteError;
                }
            }

            PrintResolvedJob(config, output_layout);
            PrintImportSummary(import_result);
            PrintAtlasSummary(atlas_result);
            PrintRayTracingSceneSummary(ray_tracing_scene);
            std::wcerr << L"Atlas validation failed. See atlas summary for details.\n";
            return kExitAtlasValidationError;
        }

        if (!config.resume)
        {
            error_message.clear();
            if (!output_writer.WriteBootstrapPackage(config, output_layout, import_result, &atlas_result, error_message))
            {
                std::wcerr << error_message << L"\n";
                return kExitOutputWriteError;
            }

            error_message.clear();
            if (!accumulator.LoadResumeState(output_layout.resume_path, resume_state, error_message))
            {
                std::wcerr << error_message << L"\n";
                return kExitResumeValidationError;
            }
        }

        error_message.clear();
        if (!accumulator.ValidateResumeState(resume_state, config, atlas_result, error_message))
        {
            PrintResolvedJob(config, output_layout);
            PrintImportSummary(import_result);
            PrintAtlasSummary(atlas_result);
            PrintRayTracingSceneSummary(ray_tracing_scene);
            PrintResumeSummary(resume_state);
            std::wcerr << error_message << L"\n";
            return kExitResumeValidationError;
        }

        BakeRayTracingRuntimeBuilder ray_tracing_runtime_builder{};
        BakeRayTracingRuntimeSettings ray_tracing_runtime_settings{};
        BakeRayTracingRuntimeBuildResult ray_tracing_runtime{};
        error_message.clear();
        const bool ray_tracing_runtime_success =
            ray_tracing_runtime_builder.BuildRuntime(ray_tracing_scene, ray_tracing_runtime_settings, ray_tracing_runtime, error_message);
        const std::wstring ray_tracing_runtime_error = error_message;

        error_message.clear();
        if (!output_writer.WriteRayTracingRuntimeSummary(ray_tracing_runtime, output_layout, error_message))
        {
            ray_tracing_runtime.Shutdown();
            std::wcerr << error_message << L"\n";
            return kExitOutputWriteError;
        }

        if (!ray_tracing_runtime_success || ray_tracing_runtime.HasValidationErrors())
        {
            PrintResolvedJob(config, output_layout);
            PrintImportSummary(import_result);
            PrintAtlasSummary(atlas_result);
            PrintRayTracingSceneSummary(ray_tracing_scene);
            PrintRayTracingRuntimeSummary(ray_tracing_runtime);
            PrintResumeSummary(resume_state);
            if (!ray_tracing_runtime_error.empty())
            {
                std::wcerr << ray_tracing_runtime_error << L"\n";
            }
            else
            {
                std::wcerr << L"Ray tracing runtime bootstrap failed. See ray tracing runtime summary for details.\n";
            }
            ray_tracing_runtime.Shutdown();
            return kExitRayTracingRuntimeError;
        }

        BakeRayTracingDispatchRunner ray_tracing_dispatch_runner{};
        BakeRayTracingDispatchSettings ray_tracing_dispatch_settings{};
        const unsigned remaining_dispatch_samples =
            resume_state.completed_samples < config.target_samples
                ? (config.target_samples - resume_state.completed_samples)
                : 0u;
        ray_tracing_dispatch_settings.sample_index = resume_state.completed_samples;
        ray_tracing_dispatch_settings.sample_count =
            config.progressive
                ? (std::min)(config.samples_per_iteration, remaining_dispatch_samples)
                : remaining_dispatch_samples;
        ray_tracing_dispatch_settings.max_bounces = config.max_bounces;
        BakeRayTracingDispatchRunResult ray_tracing_dispatch{};
        error_message.clear();
        const bool ray_tracing_dispatch_success = ray_tracing_dispatch_runner.RunDispatch(
            atlas_result,
            ray_tracing_runtime,
            ray_tracing_dispatch_settings,
            ray_tracing_dispatch,
            error_message);
        const std::wstring ray_tracing_dispatch_error = error_message;

        error_message.clear();
        if (!output_writer.WriteRayTracingDispatchSummary(ray_tracing_dispatch, output_layout, error_message))
        {
            ray_tracing_runtime.Shutdown();
            std::wcerr << error_message << L"\n";
            return kExitOutputWriteError;
        }

        ray_tracing_runtime.Shutdown();
        if (!ray_tracing_dispatch_success || ray_tracing_dispatch.HasValidationErrors())
        {
            PrintResolvedJob(config, output_layout);
            PrintImportSummary(import_result);
            PrintAtlasSummary(atlas_result);
            PrintRayTracingSceneSummary(ray_tracing_scene);
            PrintRayTracingRuntimeSummary(ray_tracing_runtime);
            PrintRayTracingDispatchSummary(ray_tracing_dispatch);
            PrintResumeSummary(resume_state);
            if (!ray_tracing_dispatch_error.empty())
            {
                std::wcerr << ray_tracing_dispatch_error << L"\n";
            }
            else
            {
                std::wcerr << L"Ray tracing dispatch bootstrap failed. See ray tracing dispatch summary for details.\n";
            }
            return kExitRayTracingDispatchError;
        }

        BakeAccumulationRunResult accumulation_run_result{};
        error_message.clear();
        if (!accumulator.AccumulateRayTracingDispatchOutput(resume_state,
                                                            config,
                                                            atlas_result,
                                                            ray_tracing_dispatch,
                                                            accumulation_run_result,
                                                            error_message))
        {
            PrintResolvedJob(config, output_layout);
            PrintImportSummary(import_result);
            PrintAtlasSummary(atlas_result);
            PrintRayTracingSceneSummary(ray_tracing_scene);
            PrintRayTracingRuntimeSummary(ray_tracing_runtime);
            PrintRayTracingDispatchSummary(ray_tracing_dispatch);
            PrintResumeSummary(resume_state);
            std::wcerr << error_message << L"\n";
            return kExitAccumulationError;
        }

        BakePackageProgressState progress_state{};
        progress_state.completed_samples = accumulation_run_result.completed_samples;
        progress_state.has_accumulation_cache = true;
        progress_state.bootstrap_initial_payload = false;
        error_message.clear();
        if (!output_writer.RefreshPackageMetadata(config,
                                                 output_layout,
                                                 import_result,
                                                 atlas_result,
                                                 progress_state,
                                                 error_message))
        {
            std::wcerr << error_message << L"\n";
            return kExitOutputWriteError;
        }

        resume_state.completed_samples = accumulation_run_result.completed_samples;
        resume_state.has_accumulation_cache = true;

        PrintResolvedJob(config, output_layout);
        PrintImportSummary(import_result);
        PrintAtlasSummary(atlas_result);
        PrintRayTracingSceneSummary(ray_tracing_scene);
        PrintRayTracingRuntimeSummary(ray_tracing_runtime);
        PrintRayTracingDispatchSummary(ray_tracing_dispatch);
        PrintResumeSummary(resume_state);
        PrintAccumulationSummary(accumulation_run_result);

        const std::filesystem::path import_summary_path = output_layout.debug / L"import_summary.json";
        const std::filesystem::path atlas_summary_path = output_layout.debug / L"atlas_summary.json";
        const std::filesystem::path ray_tracing_scene_summary_path =
            output_layout.debug / L"raytracing_scene_summary.json";
        const std::filesystem::path ray_tracing_runtime_summary_path =
            output_layout.debug / L"raytracing_runtime_summary.json";
        const std::filesystem::path ray_tracing_dispatch_summary_path =
            output_layout.debug / L"raytracing_dispatch_summary.json";
        std::wcout
            << L"\nProgressive preview sidecar package updated.\n"
            << L"  manifest: " << output_layout.manifest_path.native() << L"\n"
            << L"  resume metadata: " << output_layout.resume_path.native() << L"\n"
            << L"  import summary: " << import_summary_path.native() << L"\n"
            << L"  atlas summary: " << atlas_summary_path.native() << L"\n"
            << L"  ray tracing scene summary: " << ray_tracing_scene_summary_path.native() << L"\n"
            << L"  ray tracing runtime summary: " << ray_tracing_runtime_summary_path.native() << L"\n"
            << L"  ray tracing dispatch summary: " << ray_tracing_dispatch_summary_path.native() << L"\n"
            << L"  preview integrator: dxr atlas diffuse path tracing\n";

        return kExitSuccess;
    }

    void LightingBakerApp::PrintResolvedJob(const BakeJobConfig& config, const BakeOutputLayout& output_layout) const
    {
        std::wcout
            << L"LightingBaker job\n"
            << L"  scene: " << config.scene_path.native() << L"\n"
            << L"  output: " << output_layout.root.native() << L"\n"
            << L"  manifest: " << output_layout.manifest_path.native() << L"\n"
            << L"  resume metadata: " << output_layout.resume_path.native() << L"\n"
            << L"  atlas resolution: " << config.atlas_resolution << L"\n"
            << L"  samples / iteration: " << config.samples_per_iteration << L"\n"
            << L"  target samples: " << config.target_samples << L"\n"
            << L"  max bounces: " << config.max_bounces << L"\n"
            << L"  progressive: " << (config.progressive ? L"true" : L"false") << L"\n"
            << L"  resume: " << (config.resume ? L"true" : L"false") << L"\n";
    }

    void LightingBakerApp::PrintImportSummary(const BakeSceneImportResult& import_result) const
    {
        std::wcout
            << L"\nScene import summary\n"
            << L"  load succeeded: " << (import_result.load_succeeded ? L"true" : L"false") << L"\n"
            << L"  nodes: " << import_result.node_count << L"\n"
            << L"  meshes: " << import_result.mesh_count << L"\n"
            << L"  instance primitives: " << import_result.instance_primitive_count << L"\n"
            << L"  valid lightmap primitives: " << import_result.valid_lightmap_primitive_count << L"\n"
            << L"  punctual lights: " << import_result.punctual_light_count << L"\n"
            << L"  directional lights: " << import_result.directional_light_count << L"\n"
            << L"  point lights: " << import_result.point_light_count << L"\n"
            << L"  spot lights: " << import_result.spot_light_count << L"\n"
            << L"  warnings: " << CountWarnings(import_result) << L"\n"
            << L"  errors: " << CountErrors(import_result) << L"\n";
    }

    void LightingBakerApp::PrintAtlasSummary(const LightmapAtlasBuildResult& atlas_result) const
    {
        std::wcout
            << L"\nAtlas build summary\n"
            << L"  atlas resolution: " << atlas_result.atlas_resolution << L"\n"
            << L"  texel border: " << atlas_result.texel_border << L"\n"
            << L"  bindings: " << atlas_result.binding_count << L"\n"
            << L"  texel records: " << atlas_result.texel_record_count << L"\n"
            << L"  overlapped texels: " << atlas_result.overlapped_texel_count << L"\n"
            << L"  warnings: " << CountWarnings(atlas_result) << L"\n"
            << L"  errors: " << CountErrors(atlas_result) << L"\n";
    }

    void LightingBakerApp::PrintRayTracingSceneSummary(const BakeRayTracingSceneBuildResult& ray_tracing_scene) const
    {
        std::wcout
            << L"\nRay tracing scene summary\n"
            << L"  geometries: " << ray_tracing_scene.geometry_count << L"\n"
            << L"  instances: " << ray_tracing_scene.instance_count << L"\n"
            << L"  vertices: " << ray_tracing_scene.vertex_count << L"\n"
            << L"  indices: " << ray_tracing_scene.index_count << L"\n"
            << L"  triangles: " << ray_tracing_scene.triangle_count << L"\n"
            << L"  shading vertices: " << ray_tracing_scene.shading_vertex_count << L"\n"
            << L"  shading indices: " << ray_tracing_scene.shading_index_count << L"\n"
            << L"  shading instances: " << ray_tracing_scene.shading_instance_count << L"\n"
            << L"  scene lights: " << ray_tracing_scene.scene_light_count << L"\n"
            << L"  directional lights: " << ray_tracing_scene.directional_light_count << L"\n"
            << L"  point lights: " << ray_tracing_scene.point_light_count << L"\n"
            << L"  spot lights: " << ray_tracing_scene.spot_light_count << L"\n"
            << L"  material textures: " << ray_tracing_scene.material_texture_count << L"\n"
            << L"  normal-mapped instances: " << ray_tracing_scene.normal_mapped_instance_count << L"\n"
            << L"  metallic-roughness-textured instances: "
            << ray_tracing_scene.metallic_roughness_textured_instance_count << L"\n"
            << L"  alpha-masked instances: " << ray_tracing_scene.alpha_masked_instance_count << L"\n"
            << L"  alpha-blended instances: " << ray_tracing_scene.alpha_blended_instance_count << L"\n"
            << L"  fully transparent masked primitives: "
            << ray_tracing_scene.fully_transparent_masked_primitive_count << L"\n"
            << L"  skipped primitives: " << ray_tracing_scene.skipped_primitive_count << L"\n"
            << L"  warnings: " << CountWarnings(ray_tracing_scene) << L"\n"
            << L"  errors: " << CountErrors(ray_tracing_scene) << L"\n";
    }

    void LightingBakerApp::PrintRayTracingRuntimeSummary(const BakeRayTracingRuntimeBuildResult& ray_tracing_runtime) const
    {
        std::wcout
            << L"\nRay tracing runtime summary\n"
            << L"  device: "
            << (ray_tracing_runtime.device_type == RendererInterface::DX12 ? L"DX12" : L"VULKAN") << L"\n"
            << L"  hidden window: " << ray_tracing_runtime.hidden_window_width << L"x"
            << ray_tracing_runtime.hidden_window_height << L"\n"
            << L"  frame slots: " << ray_tracing_runtime.frame_slot_count << L"\n"
            << L"  swapchain images: " << ray_tracing_runtime.swapchain_image_count << L"\n"
            << L"  uploaded geometries: " << ray_tracing_runtime.uploaded_geometry_count << L"\n"
            << L"  uploaded instances: " << ray_tracing_runtime.uploaded_instance_count << L"\n"
            << L"  uploaded vertices: " << ray_tracing_runtime.uploaded_vertex_count << L"\n"
            << L"  uploaded indices: " << ray_tracing_runtime.uploaded_index_count << L"\n"
            << L"  uploaded shading vertices: " << ray_tracing_runtime.uploaded_shading_vertex_count << L"\n"
            << L"  uploaded shading indices: " << ray_tracing_runtime.uploaded_shading_index_count << L"\n"
            << L"  uploaded shading instances: " << ray_tracing_runtime.uploaded_shading_instance_count << L"\n"
            << L"  uploaded scene lights: " << ray_tracing_runtime.uploaded_scene_light_count << L"\n"
            << L"  uploaded material textures: " << ray_tracing_runtime.uploaded_material_texture_count << L"\n"
            << L"  bound material textures: " << ray_tracing_runtime.bound_material_texture_count << L"\n"
            << L"  window created: " << (ray_tracing_runtime.window_created ? L"true" : L"false") << L"\n"
            << L"  resource operator ready: "
            << (ray_tracing_runtime.resource_operator_initialized ? L"true" : L"false") << L"\n"
            << L"  acceleration structure ready: "
            << (ray_tracing_runtime.acceleration_structure_initialized ? L"true" : L"false") << L"\n"
            << L"  scene vertex buffer created: " << (ray_tracing_runtime.scene_vertex_buffer_created ? L"true" : L"false") << L"\n"
            << L"  scene index buffer created: " << (ray_tracing_runtime.scene_index_buffer_created ? L"true" : L"false") << L"\n"
            << L"  scene instance buffer created: " << (ray_tracing_runtime.scene_instance_buffer_created ? L"true" : L"false") << L"\n"
            << L"  scene light buffer created: " << (ray_tracing_runtime.scene_light_buffer_created ? L"true" : L"false") << L"\n"
            << L"  material texture table created: " << (ray_tracing_runtime.material_texture_table_created ? L"true" : L"false") << L"\n"
            << L"  fallback material texture created: " << (ray_tracing_runtime.fallback_material_texture_created ? L"true" : L"false") << L"\n"
            << L"  warnings: " << CountWarnings(ray_tracing_runtime) << L"\n"
            << L"  errors: " << CountErrors(ray_tracing_runtime) << L"\n";
    }

    void LightingBakerApp::PrintRayTracingDispatchSummary(const BakeRayTracingDispatchRunResult& ray_tracing_dispatch) const
    {
        std::wcout
            << L"\nRay tracing dispatch summary\n"
            << L"  shader resolved: " << (ray_tracing_dispatch.shader_path_resolved ? L"true" : L"false") << L"\n"
            << L"  any-hit entry: " << to_wide_string(ray_tracing_dispatch.any_hit_entry) << L"\n"
            << L"  sample index: " << ray_tracing_dispatch.sample_index << L"\n"
            << L"  sample count: " << ray_tracing_dispatch.sample_count << L"\n"
            << L"  max bounces: " << ray_tracing_dispatch.max_bounces << L"\n"
            << L"  dispatch: " << ray_tracing_dispatch.dispatch_width << L"x"
            << ray_tracing_dispatch.dispatch_height << L"x" << ray_tracing_dispatch.dispatch_depth << L"\n"
            << L"  dense texel records: " << ray_tracing_dispatch.dense_texel_record_count << L"\n"
            << L"  scene lights: " << ray_tracing_dispatch.scene_light_count << L"\n"
            << L"  output: " << ray_tracing_dispatch.output_width << L"x"
            << ray_tracing_dispatch.output_height << L"\n"
            << L"  accel-structure root allocation found: "
            << (ray_tracing_dispatch.acceleration_structure_root_allocation_found ? L"true" : L"false") << L"\n"
            << L"  texel-record root allocation found: "
            << (ray_tracing_dispatch.texel_record_root_allocation_found ? L"true" : L"false") << L"\n"
            << L"  scene-vertex root allocation found: "
            << (ray_tracing_dispatch.scene_vertex_root_allocation_found ? L"true" : L"false") << L"\n"
            << L"  scene-index root allocation found: "
            << (ray_tracing_dispatch.scene_index_root_allocation_found ? L"true" : L"false") << L"\n"
            << L"  scene-instance root allocation found: "
            << (ray_tracing_dispatch.scene_instance_root_allocation_found ? L"true" : L"false") << L"\n"
            << L"  scene-light root allocation found: "
            << (ray_tracing_dispatch.scene_light_root_allocation_found ? L"true" : L"false") << L"\n"
            << L"  material-texture root allocation found: "
            << (ray_tracing_dispatch.material_texture_root_allocation_found ? L"true" : L"false") << L"\n"
            << L"  dispatch-constants root allocation found: "
            << (ray_tracing_dispatch.dispatch_constants_root_allocation_found ? L"true" : L"false") << L"\n"
            << L"  output root allocation found: "
            << (ray_tracing_dispatch.output_root_allocation_found ? L"true" : L"false") << L"\n"
            << L"  texel record buffer created: "
            << (ray_tracing_dispatch.texel_record_buffer_created ? L"true" : L"false") << L"\n"
            << L"  scene vertex buffer bound: " << (ray_tracing_dispatch.scene_vertex_buffer_bound ? L"true" : L"false") << L"\n"
            << L"  scene index buffer bound: " << (ray_tracing_dispatch.scene_index_buffer_bound ? L"true" : L"false") << L"\n"
            << L"  scene instance buffer bound: " << (ray_tracing_dispatch.scene_instance_buffer_bound ? L"true" : L"false") << L"\n"
            << L"  scene light buffer bound: " << (ray_tracing_dispatch.scene_light_buffer_bound ? L"true" : L"false") << L"\n"
            << L"  material texture table bound: " << (ray_tracing_dispatch.material_texture_table_bound ? L"true" : L"false") << L"\n"
            << L"  dispatch constants buffer created: "
            << (ray_tracing_dispatch.dispatch_constants_buffer_created ? L"true" : L"false") << L"\n"
            << L"  render pass created: " << (ray_tracing_dispatch.render_pass_created ? L"true" : L"false") << L"\n"
            << L"  shader table ready: " << (ray_tracing_dispatch.shader_table_initialized ? L"true" : L"false") << L"\n"
            << L"  pre-dispatch uploads flushed: "
            << (ray_tracing_dispatch.pre_dispatch_uploads_flushed ? L"true" : L"false") << L"\n"
            << L"  frame executed: " << (ray_tracing_dispatch.frame_executed ? L"true" : L"false") << L"\n"
            << L"  readback buffer allocated: " << (ray_tracing_dispatch.readback_buffer_allocated ? L"true" : L"false") << L"\n"
            << L"  readback downloaded: " << (ray_tracing_dispatch.readback_downloaded ? L"true" : L"false") << L"\n"
            << L"  output payload valid: " << (ray_tracing_dispatch.output_payload_valid ? L"true" : L"false") << L"\n"
            << L"  output row pitch: " << ray_tracing_dispatch.output_row_pitch << L"\n"
            << L"  nonzero rgb texels: " << ray_tracing_dispatch.output_nonzero_rgb_texel_count << L"\n"
            << L"  nonzero alpha texels: " << ray_tracing_dispatch.output_nonzero_alpha_texel_count << L"\n"
            << L"  payload sentinel texels: "
            << ray_tracing_dispatch.output_trace_payload_sentinel_texel_count << L"\n"
            << L"  executed RT passes: " << ray_tracing_dispatch.frame_stats.executed_ray_tracing_pass_count << L"\n"
            << L"  warnings: " << CountWarnings(ray_tracing_dispatch) << L"\n"
            << L"  errors: " << CountErrors(ray_tracing_dispatch) << L"\n";
    }

    void LightingBakerApp::PrintResumeSummary(const BakeAccumulatorResumeState& resume_state) const
    {
        std::wcout
            << L"\nResume cache summary\n"
            << L"  resume metadata: " << resume_state.resume_path.native() << L"\n"
            << L"  source scene: " << resume_state.source_scene.native() << L"\n"
            << L"  completed samples: " << resume_state.completed_samples << L"\n"
            << L"  target samples: " << resume_state.target_samples << L"\n"
            << L"  atlas inputs: " << resume_state.atlas_inputs.size() << L"\n"
            << L"  has accumulation cache: " << (resume_state.has_accumulation_cache ? L"true" : L"false") << L"\n";
    }

    void LightingBakerApp::PrintAccumulationSummary(const BakeAccumulationRunResult& run_result) const
    {
        std::wcout
            << L"\nAccumulation summary\n"
            << L"  previous completed samples: " << run_result.previous_completed_samples << L"\n"
            << L"  samples added this run: " << run_result.added_samples << L"\n"
            << L"  completed samples: " << run_result.completed_samples << L"\n"
            << L"  target samples: " << run_result.target_samples << L"\n"
            << L"  cache files updated: " << (run_result.cache_files_updated ? L"true" : L"false") << L"\n"
            << L"  published atlases updated: " << (run_result.published_atlases_updated ? L"true" : L"false") << L"\n";
    }
}
