#pragma once
#include <optional>

#include "IRHIDevice.h"
#include "VolkUtils.h"

// Check queue family valid
struct QueueFamilyIndices
{
    std::optional<unsigned> graphics_family;
    std::optional<unsigned> present_family;

    bool IsComplete() const
    {
        return graphics_family.has_value() && present_family.has_value();
    }
};

struct SwapChainSupportDetails
{
    VkSurfaceCapabilitiesKHR capabilities;
    std::vector<VkSurfaceFormatKHR> formats;
    std::vector<VkPresentModeKHR> present_modes;
};

class VKDevice : public IRHIDevice
{
public:
    IMPL_NON_COPYABLE_AND_DEFAULT_CTOR_VDTOR(VKDevice)
    
    virtual bool InitDevice(IRHIFactory& factory) override;
    
    VkInstance GetInstance() const {return instance; }
    VkSurfaceKHR GetSurface() const {return surface; }
    VkPhysicalDevice GetPhysicalDevice() const {return selected_physical_device; }
    VkDevice GetDevice() const {return logical_device;}
    
    unsigned GetGraphicsQueueIndex() const {return graphics_queue_index; }
    unsigned GetPresentQueueIndex() const {return present_queue_index; }

    virtual bool Release(IRHIMemoryManager& memory_manager) override;
    
protected:
    QueueFamilyIndices FindQueueFamily(VkPhysicalDevice device, VkSurfaceKHR surface);
    SwapChainSupportDetails QuerySwapChainSupport(VkPhysicalDevice device, VkSurfaceKHR surface);
    bool IsSuitableDevice(VkPhysicalDevice device, VkSurfaceKHR surface);
    
    VkInstance instance {VK_NULL_HANDLE};
    VkSurfaceKHR surface {VK_NULL_HANDLE};
    VkPhysicalDevice selected_physical_device {VK_NULL_HANDLE};
    VkDevice logical_device {VK_NULL_HANDLE};

    unsigned graphics_queue_index {0};
    unsigned present_queue_index {0};
};
