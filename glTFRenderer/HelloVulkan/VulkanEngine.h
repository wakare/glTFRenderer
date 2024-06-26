#pragma once

#define VK_USE_PLATFORM_WIN32_KHR
#include <vulkan/vulkan.h>
#include <vulkan/vulkan_core.h>

#include <vector>

#include "vma/vk_mem_alloc.h"

struct AllocatedImage
{
    VkImage image;
    VkImageView image_view;
    VmaAllocation allocation;
    VkExtent3D image_extent;
    VkFormat image_format;
};

class VulkanEngine
{
public:
    bool Init();
    bool Update();
    bool UnInit();

protected:
    void RecordCommandBufferForDrawTestTriangle(VkCommandBuffer command_buffer, unsigned image_index);
    void RecordCommandBufferForDynamicRendering(VkCommandBuffer command_buffer, unsigned image_index);
    void DrawFrame();
    void CreateSwapChainAndRelativeResource();
    void CleanupSwapChain();
    void RecreateSwapChain();
    void TransitionImage(VkCommandBuffer cmd, VkImage image, VkImageLayout old_layout, VkImageLayout new_layout);
    
    VkInstance instance {VK_NULL_HANDLE};
    VkPhysicalDevice select_physical_device {VK_NULL_HANDLE};
    VkDevice logical_device {VK_NULL_HANDLE};
    VkQueue graphics_queue {VK_NULL_HANDLE};
    VkSurfaceKHR surface {VK_NULL_HANDLE};
    VkQueue present_queue {VK_NULL_HANDLE};
    VkSwapchainKHR swap_chain {VK_NULL_HANDLE};
    std::vector<VkImage> images;
    VkFormat swap_chain_format {VK_FORMAT_UNDEFINED};
    VkExtent2D swap_chain_extent {0, 0};
    std::vector<VkImageView> image_views;
    VkShaderModule vertex_shader_module {VK_NULL_HANDLE};
    VkShaderModule fragment_shader_module {VK_NULL_HANDLE};
    VkPipelineLayout pipeline_layout {VK_NULL_HANDLE};
    VkRenderPass render_pass {VK_NULL_HANDLE};
    VkPipeline pipeline {VK_NULL_HANDLE};
    std::vector<VkFramebuffer> swap_chain_frame_buffers;
    VkCommandPool command_pool {VK_NULL_HANDLE};
    std::vector<VkCommandBuffer> command_buffers;
    VkCommandBufferBeginInfo begin_info {};

    std::vector<VkSemaphore> image_available_semaphores;
    std::vector<VkSemaphore> render_finish_semaphores;
    std::vector<VkFence> in_flight_fences;

    unsigned current_frame_clipped {0};
    unsigned current_frame_real {0};
    bool window_resized {false};
    bool test_triangle {false};

    VmaAllocator vma_allocator {};
    AllocatedImage draw_image {};
    VkExtent2D draw_extent {};
};
