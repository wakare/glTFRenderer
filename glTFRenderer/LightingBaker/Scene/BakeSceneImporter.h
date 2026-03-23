#pragma once

#include <filesystem>

namespace LightingBaker
{
    struct BakeSceneImportRequest
    {
        std::filesystem::path scene_path;
    };

    class BakeSceneImporter
    {
    public:
        BakeSceneImporter() = default;
    };
}
