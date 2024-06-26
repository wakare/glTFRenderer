#pragma once

#define VK_USE_PLATFORM_WIN32_KHR
#include <vulkan/vulkan.h>
#include <vulkan/vulkan_core.h>

#include <vector>

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
    
    VkInstance instance;
    VkPhysicalDevice select_physical_device;
    VkDevice logical_device;
    VkQueue graphics_queue;
    VkSurfaceKHR surface;
    VkQueue present_queue;
    VkSwapchainKHR swap_chain;
    std::vector<VkImage> images;
    VkFormat swap_chain_format;
    VkExtent2D swap_chain_extent;
    std::vector<VkImageView> image_views;
    VkShaderModule vertex_shader_module;
    VkShaderModule fragment_shader_module;
    VkPipelineLayout pipeline_layout;
    VkRenderPass render_pass;
    VkPipeline pipeline;
    std::vector<VkFramebuffer> swap_chain_frame_buffers;
    VkCommandPool command_pool;
    std::vector<VkCommandBuffer> command_buffers;
    VkCommandBufferBeginInfo begin_info;

    std::vector<VkSemaphore> image_available_semaphores;
    std::vector<VkSemaphore> render_finish_semaphores;
    std::vector<VkFence> in_flight_fences;

    unsigned current_frame_clipped {0};
    unsigned current_frame_real {0};
    bool window_resized {false};
    bool test_triangle {false};
};
