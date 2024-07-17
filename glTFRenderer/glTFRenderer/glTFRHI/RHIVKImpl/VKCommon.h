#include <cassert>

#define VK_CHECK(x) {\
VkResult result = (x);\
assert(result == VK_SUCCESS);\
}