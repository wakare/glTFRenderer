#pragma once
#include <stdexcept>

enum RHICommonEnum
{
    RHI_Unknown = 123456,
};

enum class RHIDataFormat
{
    R8G8B8A8_UNORM,
    R8G8B8A8_UNORM_SRGB,
    R32G32B32_FLOAT,
    R32G32B32A32_FLOAT,
    D32_FLOAT,
    Unknown,
};

#define REGISTER_INDEX_TYPE unsigned 

#define THROW_IF_FAILED(x) \
    if (FAILED((x))) \
    { \
        throw std::runtime_error("[ERROR] Detected runtime error in call"); \
    }