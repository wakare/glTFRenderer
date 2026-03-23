#pragma once

namespace LightingBaker
{
    struct LightmapAtlasBuildSettings
    {
        unsigned atlas_resolution = 1024;
        unsigned texel_border = 4;
    };

    class LightmapAtlasBuilder
    {
    public:
        LightmapAtlasBuilder() = default;
    };
}
