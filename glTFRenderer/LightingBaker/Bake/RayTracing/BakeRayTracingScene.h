#pragma once

#include "Scene/BakeSceneImporter.h"

#include <string>
#include <vector>

#include "RHICommon.h"
#include "RHIInterface/IRHIRayTracingAS.h"

namespace LightingBaker
{
    struct BakeRayTracingGeometrySource
    {
        unsigned geometry_index{0u};
        unsigned stable_node_key{0xffffffffu};
        unsigned primitive_hash{0xffffffffu};
        unsigned material_index{0xffffffffu};
        std::size_t vertex_count{0u};
        std::size_t index_count{0u};
        std::size_t triangle_count{0u};
        bool opaque{true};
        std::string node_name{};
        std::string mesh_name{};
        VertexBufferData vertex_buffer_data{};
        IndexBufferData index_buffer_data{};
    };

    struct BakeRayTracingSceneBuildResult
    {
        std::size_t geometry_count{0u};
        std::size_t instance_count{0u};
        std::size_t vertex_count{0u};
        std::size_t index_count{0u};
        std::size_t triangle_count{0u};
        std::size_t skipped_primitive_count{0u};
        std::vector<BakeRayTracingGeometrySource> geometries{};
        std::vector<RHIRayTracingInstanceDesc> instances{};
        std::vector<BakeSceneValidationMessage> errors{};
        std::vector<BakeSceneValidationMessage> warnings{};

        bool HasErrors() const;
        bool HasValidationErrors() const;
    };

    class BakeRayTracingSceneBuilder
    {
    public:
        bool BuildScene(const BakeSceneImportResult& import_result,
                        BakeRayTracingSceneBuildResult& out_result,
                        std::wstring& out_error) const;
    };
}
