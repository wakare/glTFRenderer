#pragma once

#include"RendererCommon.h"

#define VK_CHECK(x) {\
VkResult result = (x);\
if (result != VK_SUCCESS)\
{LOG_FORMAT_FLUSH("Vk failed with code: %d", (int)result); GLTF_CHECK(false);}\
}