#include "BakeRayTracingRuntime.h"

#include <Windows.h>

#include <algorithm>
#include <array>
#include <utility>

#include "RHIResourceFactoryImpl.hpp"

namespace LightingBaker
{
    namespace
    {
        void FlushRecordedCommands(RendererInterface::ResourceOperator& resource_operator)
        {
            resource_operator.WaitFrameRenderFinished();
            resource_operator.WaitGPUIdle();
        }

        BakeSceneValidationMessage MakeMessage(std::string code, std::string message)
        {
            return {
                .code = std::move(code),
                .message = std::move(message)
            };
        }

        RendererInterface::TextureDesc CreateFallbackMaterialTextureDesc()
        {
            constexpr std::array<char, 4u> kFallbackMaterialTextureRgba{
                static_cast<char>(255),
                static_cast<char>(255),
                static_cast<char>(255),
                static_cast<char>(255)
            };

            RendererInterface::TextureDesc texture_desc{};
            texture_desc.format = RendererInterface::RGBA8_UNORM;
            texture_desc.width = 1u;
            texture_desc.height = 1u;
            texture_desc.data.assign(
                kFallbackMaterialTextureRgba.begin(),
                kFallbackMaterialTextureRgba.end());
            return texture_desc;
        }

        RHIBufferDesc BuildVertexBufferDesc(const BakeRayTracingGeometrySource& geometry_source)
        {
            return {
                L"LightingBaker_RT_VertexBuffer",
                geometry_source.vertex_buffer_data.byte_size,
                1,
                1,
                RHIBufferType::Default,
                RHIBufferResourceType::Buffer,
                RHIResourceStateType::STATE_COMMON,
                static_cast<RHIResourceUsageFlags>(RUF_VERTEX_BUFFER | RUF_TRANSFER_DST | RUF_RAY_TRACING)
            };
        }

        RHIBufferDesc BuildIndexBufferDesc(const BakeRayTracingGeometrySource& geometry_source)
        {
            return {
                L"LightingBaker_RT_IndexBuffer",
                geometry_source.index_buffer_data.byte_size,
                1,
                1,
                RHIBufferType::Default,
                RHIBufferResourceType::Buffer,
                RHIResourceStateType::STATE_COMMON,
                static_cast<RHIResourceUsageFlags>(RUF_INDEX_BUFFER | RUF_TRANSFER_DST | RUF_RAY_TRACING)
            };
        }

        template<typename T>
        bool CreateStructuredSceneBuffer(RendererInterface::ResourceOperator& resource_operator,
                                         const char* name,
                                         const std::vector<T>& data,
                                         RendererInterface::BufferHandle& out_handle)
        {
            out_handle = {};
            if (data.empty())
            {
                return false;
            }

            RendererInterface::BufferDesc buffer_desc{};
            buffer_desc.name = name;
            buffer_desc.usage = RendererInterface::USAGE_SRV;
            buffer_desc.type = RendererInterface::DEFAULT;
            buffer_desc.size = data.size() * sizeof(T);
            buffer_desc.data = const_cast<T*>(data.data());
            out_handle = resource_operator.CreateBuffer(buffer_desc);
            return out_handle.IsValid();
        }
    }

    bool BakeRayTracingRuntimeBuildResult::HasErrors() const
    {
        return !errors.empty();
    }

    bool BakeRayTracingRuntimeBuildResult::HasValidationErrors() const
    {
        return HasErrors();
    }

    void BakeRayTracingRuntimeBuildResult::Shutdown()
    {
        acceleration_structure.reset();
        scene_vertex_buffer_handle = {};
        scene_index_buffer_handle = {};
        scene_instance_buffer_handle = {};
        scene_light_buffer_handle = {};
        emissive_triangle_buffer_handle = {};
        material_texture_handles.clear();
        vertex_buffers.clear();
        index_buffers.clear();

        if (resource_operator)
        {
            resource_operator->CleanupAllResources(true);
            resource_operator.reset();
        }

        window.reset();
    }

    bool BakeRayTracingRuntimeBuilder::BuildRuntime(const BakeRayTracingSceneBuildResult& ray_tracing_scene,
                                                    const BakeRayTracingRuntimeSettings& settings,
                                                    BakeRayTracingRuntimeBuildResult& out_result,
                                                    std::wstring& out_error) const
    {
        out_result.Shutdown();
        out_result = BakeRayTracingRuntimeBuildResult{};
        out_result.device_type = settings.device_type;
        out_result.hidden_window_width = settings.hidden_window_width;
        out_result.hidden_window_height = settings.hidden_window_height;
        out_error.clear();

        if (ray_tracing_scene.HasValidationErrors())
        {
            out_result.errors.push_back(MakeMessage(
                "rt_runtime_invalid_scene",
                "Ray tracing runtime bootstrap requires a validation-passed ray tracing scene."));
            out_error = L"Ray tracing runtime bootstrap requires a validation-passed ray tracing scene.";
            return false;
        }

        if (ray_tracing_scene.geometries.empty() || ray_tracing_scene.instances.empty())
        {
            out_result.errors.push_back(MakeMessage(
                "rt_runtime_empty_scene",
                "Ray tracing runtime bootstrap requires at least one geometry and one instance."));
            out_error = L"Ray tracing runtime bootstrap requires at least one geometry and one instance.";
            return false;
        }

        RendererInterface::RenderWindowDesc window_desc{};
        window_desc.width = (std::max)(1u, settings.hidden_window_width);
        window_desc.height = (std::max)(1u, settings.hidden_window_height);
        out_result.window = std::make_shared<RendererInterface::RenderWindow>(window_desc);
        out_result.window_created = true;
        if (const HWND hwnd = out_result.window->GetHWND(); hwnd != nullptr)
        {
            ::ShowWindow(hwnd, SW_HIDE);
        }

        RendererInterface::RenderDeviceDesc device_desc{};
        device_desc.window = out_result.window->GetHandle();
        device_desc.type = settings.device_type;
        device_desc.back_buffer_count = (std::max)(1u, settings.back_buffer_count);
        device_desc.vulkan_optional_capabilities.require_ray_tracing_pipeline =
            settings.device_type == RendererInterface::VULKAN;

        out_result.resource_operator = std::make_shared<RendererInterface::ResourceOperator>(device_desc);
        out_result.resource_operator_initialized = true;

        RendererInterface::ResourceOperator& resource_operator = *out_result.resource_operator;
        resource_operator.BeginFrame();
        IRHIDevice& device = resource_operator.GetDevice();
        IRHIMemoryManager& memory_manager = resource_operator.GetMemoryManager();
        IRHICommandList& command_list = resource_operator.GetCommandListForRecordPassCommand();

        RHIRayTracingSceneDesc scene_desc{};
        scene_desc.geometries.reserve(ray_tracing_scene.geometries.size());
        scene_desc.instances.reserve(ray_tracing_scene.instances.size());

        out_result.vertex_buffers.reserve(ray_tracing_scene.geometries.size());
        out_result.index_buffers.reserve(ray_tracing_scene.geometries.size());

        for (const BakeRayTracingGeometrySource& geometry_source : ray_tracing_scene.geometries)
        {
            if (geometry_source.vertex_count == 0u || geometry_source.index_count == 0u)
            {
                out_result.errors.push_back(MakeMessage(
                    "rt_runtime_empty_geometry",
                    "Encountered a ray tracing geometry source without vertex or index data."));
                continue;
            }

            auto vertex_buffer = std::make_shared<RHIVertexBuffer>();
            auto index_buffer = std::make_shared<RHIIndexBuffer>();

            auto vertex_buffer_view = vertex_buffer->CreateVertexBufferView(
                device,
                memory_manager,
                command_list,
                BuildVertexBufferDesc(geometry_source),
                geometry_source.vertex_buffer_data);
            auto index_buffer_view = index_buffer->CreateIndexBufferView(
                device,
                memory_manager,
                command_list,
                BuildIndexBufferDesc(geometry_source),
                geometry_source.index_buffer_data);

            if (!vertex_buffer_view || !index_buffer_view)
            {
                out_result.errors.push_back(MakeMessage(
                    "rt_runtime_upload_failed",
                    "Failed to upload one or more ray tracing geometry buffers."));
                continue;
            }

            index_buffer->GetBuffer().Transition(command_list, RHIResourceStateType::STATE_INDEX_BUFFER);

            scene_desc.geometries.push_back({
                .vertex_buffer = vertex_buffer,
                .index_buffer = index_buffer,
                .vertex_count = geometry_source.vertex_count,
                .index_count = geometry_source.index_count,
                .opaque = geometry_source.opaque
            });

            out_result.vertex_buffers.push_back(vertex_buffer);
            out_result.index_buffers.push_back(index_buffer);
            out_result.uploaded_vertex_count += geometry_source.vertex_count;
            out_result.uploaded_index_count += geometry_source.index_count;
        }

        if (out_result.HasValidationErrors())
        {
            out_error = L"Ray tracing runtime bootstrap failed while uploading geometry buffers.";
            FlushRecordedCommands(resource_operator);
            return false;
        }

        scene_desc.instances = ray_tracing_scene.instances;
        out_result.uploaded_geometry_count = scene_desc.geometries.size();
        out_result.uploaded_instance_count = scene_desc.instances.size();
        out_result.frame_slot_count = resource_operator.GetFrameSlotCount();
        out_result.swapchain_image_count = resource_operator.GetSwapchainImageCount();
        out_result.uploaded_shading_vertex_count = ray_tracing_scene.shading_vertex_count;
        out_result.uploaded_shading_index_count = ray_tracing_scene.shading_index_count;
        out_result.uploaded_shading_instance_count = ray_tracing_scene.shading_instance_count;
        out_result.uploaded_material_texture_count = ray_tracing_scene.material_texture_count;

        if (scene_desc.geometries.empty() || scene_desc.instances.empty())
        {
            out_result.errors.push_back(MakeMessage(
                "rt_runtime_scene_upload_empty",
                "Ray tracing runtime bootstrap uploaded an empty scene."));
            out_error = L"Ray tracing runtime bootstrap uploaded an empty scene.";
            FlushRecordedCommands(resource_operator);
            return false;
        }

        if (ray_tracing_scene.shading_vertices.empty() ||
            ray_tracing_scene.shading_indices.empty() ||
            ray_tracing_scene.shading_instances.empty())
        {
            out_result.errors.push_back(MakeMessage(
                "rt_runtime_shading_scene_empty",
                "Ray tracing runtime bootstrap requires non-empty shading buffers for the baker hit shader."));
            out_error = L"Ray tracing runtime bootstrap requires non-empty shading buffers for the baker hit shader.";
            FlushRecordedCommands(resource_operator);
            return false;
        }

        if (!CreateStructuredSceneBuffer(resource_operator,
                                         "LightingBaker_RT_SceneVertices",
                                         ray_tracing_scene.shading_vertices,
                                         out_result.scene_vertex_buffer_handle))
        {
            out_result.errors.push_back(MakeMessage(
                "rt_runtime_scene_vertex_buffer_failed",
                "Failed to allocate the baker shading vertex buffer."));
            out_error = L"Failed to allocate the baker shading vertex buffer.";
            FlushRecordedCommands(resource_operator);
            return false;
        }

        if (!CreateStructuredSceneBuffer(resource_operator,
                                         "LightingBaker_RT_SceneIndices",
                                         ray_tracing_scene.shading_indices,
                                         out_result.scene_index_buffer_handle))
        {
            out_result.errors.push_back(MakeMessage(
                "rt_runtime_scene_index_buffer_failed",
                "Failed to allocate the baker shading index buffer."));
            out_error = L"Failed to allocate the baker shading index buffer.";
            FlushRecordedCommands(resource_operator);
            return false;
        }

        if (!CreateStructuredSceneBuffer(resource_operator,
                                         "LightingBaker_RT_SceneInstances",
                                         ray_tracing_scene.shading_instances,
                                         out_result.scene_instance_buffer_handle))
        {
            out_result.errors.push_back(MakeMessage(
                "rt_runtime_scene_instance_buffer_failed",
                "Failed to allocate the baker shading instance buffer."));
            out_error = L"Failed to allocate the baker shading instance buffer.";
            FlushRecordedCommands(resource_operator);
            return false;
        }

        std::vector<BakeRayTracingSceneLightGPU> scene_light_buffer_data = ray_tracing_scene.scene_lights;
        if (scene_light_buffer_data.empty())
        {
            scene_light_buffer_data.push_back(BakeRayTracingSceneLightGPU{
                .position_and_type = {0.0f, 0.0f, 0.0f, static_cast<float>(BakeRayTracingSceneLightTypePoint)},
                .direction_and_range = {0.0f, 0.0f, -1.0f, -1.0f},
                .color_and_intensity = {0.0f, 0.0f, 0.0f, 0.0f},
                .spot_angles = {1.0f, 0.0f, 0.0f, 0.0f},
            });
        }

        if (!CreateStructuredSceneBuffer(resource_operator,
                                         "LightingBaker_RT_SceneLights",
                                         scene_light_buffer_data,
                                         out_result.scene_light_buffer_handle))
        {
            out_result.errors.push_back(MakeMessage(
                "rt_runtime_scene_light_buffer_failed",
                "Failed to allocate the baker scene light buffer."));
            out_error = L"Failed to allocate the baker scene light buffer.";
            FlushRecordedCommands(resource_operator);
            return false;
        }

        out_result.scene_vertex_buffer_created = true;
        out_result.scene_index_buffer_created = true;
        out_result.scene_instance_buffer_created = true;
        out_result.scene_light_buffer_created = !ray_tracing_scene.scene_lights.empty();
        out_result.uploaded_scene_light_count = ray_tracing_scene.scene_lights.size();

        std::vector<BakeRayTracingSceneEmissiveTriangleGPU> emissive_triangle_buffer_data =
            ray_tracing_scene.emissive_triangles;
        if (emissive_triangle_buffer_data.empty())
        {
            emissive_triangle_buffer_data.push_back(BakeRayTracingSceneEmissiveTriangleGPU{
                .instance_and_primitive = {0u, 0u, 0u, 0u},
                .area_pdf_cdf_luminance = {0.0f, 0.0f, 1.0f, 0.0f},
            });
        }

        if (!CreateStructuredSceneBuffer(resource_operator,
                                         "LightingBaker_RT_EmissiveTriangles",
                                         emissive_triangle_buffer_data,
                                         out_result.emissive_triangle_buffer_handle))
        {
            out_result.errors.push_back(MakeMessage(
                "rt_runtime_emissive_triangle_buffer_failed",
                "Failed to allocate the baker emissive triangle buffer."));
            out_error = L"Failed to allocate the baker emissive triangle buffer.";
            FlushRecordedCommands(resource_operator);
            return false;
        }

        out_result.emissive_triangle_buffer_created = !ray_tracing_scene.emissive_triangles.empty();
        out_result.uploaded_emissive_triangle_count = ray_tracing_scene.emissive_triangles.size();

        out_result.material_texture_handles.clear();
        out_result.material_texture_handles.reserve(ray_tracing_scene.material_texture_uris.size());
        for (const std::string& texture_uri : ray_tracing_scene.material_texture_uris)
        {
            RendererInterface::TextureFileDesc texture_desc{};
            texture_desc.uri = texture_uri;
            const RendererInterface::TextureHandle texture_handle = resource_operator.CreateTexture(texture_desc);
            if (!texture_handle.IsValid())
            {
                out_result.errors.push_back(MakeMessage(
                    "rt_runtime_material_texture_failed",
                    "Failed to allocate baker material texture resource."));
                out_error = L"Failed to allocate a baker material texture resource.";
                FlushRecordedCommands(resource_operator);
                return false;
            }

            out_result.material_texture_handles.push_back(texture_handle);
        }
        if (out_result.material_texture_handles.empty())
        {
            const RendererInterface::TextureHandle fallback_texture_handle =
                resource_operator.CreateTexture(CreateFallbackMaterialTextureDesc());
            if (!fallback_texture_handle.IsValid())
            {
                out_result.errors.push_back(MakeMessage(
                    "rt_runtime_material_texture_fallback_failed",
                    "Failed to allocate the baker fallback material texture resource."));
                out_error = L"Failed to allocate the baker fallback material texture resource.";
                FlushRecordedCommands(resource_operator);
                return false;
            }

            out_result.material_texture_handles.push_back(fallback_texture_handle);
            out_result.fallback_material_texture_created = true;
        }
        out_result.bound_material_texture_count = out_result.material_texture_handles.size();
        out_result.material_texture_table_created = !out_result.material_texture_handles.empty();

        out_result.acceleration_structure = RHIResourceFactory::CreateRHIResource<IRHIRayTracingAS>();
        if (!out_result.acceleration_structure)
        {
            out_result.errors.push_back(MakeMessage(
                "rt_runtime_as_allocation_failed",
                "Failed to allocate an acceleration structure object for the baker runtime."));
            out_error = L"Failed to allocate a ray tracing acceleration structure object.";
            FlushRecordedCommands(resource_operator);
            return false;
        }

        out_result.acceleration_structure->SetRayTracingSceneDesc(scene_desc);
        if (!out_result.acceleration_structure->InitRayTracingAS(device, command_list, memory_manager))
        {
            out_result.errors.push_back(MakeMessage(
                "rt_runtime_as_init_failed",
                "Failed to build baker ray tracing acceleration structures."));
            out_error = L"Failed to initialize baker ray tracing acceleration structures.";
            FlushRecordedCommands(resource_operator);
            return false;
        }

        out_result.acceleration_structure_initialized = true;
        FlushRecordedCommands(resource_operator);
        return true;
    }
}
