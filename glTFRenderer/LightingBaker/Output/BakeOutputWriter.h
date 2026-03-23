#pragma once

#include <filesystem>

namespace LightingBaker
{
    struct BakeOutputLayout
    {
        std::filesystem::path root;
        std::filesystem::path atlases;
        std::filesystem::path debug;
        std::filesystem::path cache;
    };

    class BakeOutputWriter
    {
    public:
        BakeOutputWriter() = default;
    };
}
