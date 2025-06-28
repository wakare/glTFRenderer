#pragma once

#include <volk.h>

class VolkUtils
{
public:
    static bool InitVolk();
    static bool LoadDevice(VkDevice device);
    static bool LoadInstance(VkInstance instance);
};
