#pragma once
#include <stdexcept>

enum RHICommonEnum
{
    RHI_Unknown = 123456,
};

#define REGISTER_INDEX_TYPE unsigned 

#define THROW_IF_FAILED(x) \
    if (FAILED((x))) \
    { \
        throw std::runtime_error("[ERROR] Detected runtime error in call"); \
    }