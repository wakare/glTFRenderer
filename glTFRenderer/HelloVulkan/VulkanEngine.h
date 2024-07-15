#pragma once

#define VK_USE_PLATFORM_WIN32_KHR
#include <functional>
#include <vulkan/vulkan_core.h>

#include <vector>
#include <span>
#include <glm/glm.hpp>
#include <vma/vk_mem_alloc.h>

#include "DescriptorHelper.h"

struct AllocatedImage
{
    VkImage image;
    VkImageView image_view;
    VmaAllocation allocation;
    VkExtent3D image_extent;
    VkFormat image_format;
};

struct AllocatedBuffer {
    VkBuffer buffer;
    VmaAllocation allocation;
    VmaAllocationInfo info;
};

struct Vertex {

    glm::vec3 position;
    float uv_x;
    glm::vec3 normal;
    float uv_y;
    glm::vec4 color;
};

// holds the resources needed for a mesh
struct GPUMeshBuffers {
    AllocatedBuffer indexBuffer;
    AllocatedBuffer vertexBuffer;
    VkDeviceAddress vertexBufferAddress;
};

// push constants for our mesh object draws
struct GPUDrawPushConstants {
    glm::mat4 worldMatrix;
    VkDeviceAddress vertexBuffer;
};

struct GPUSceneData {
    glm::mat4 view;
    glm::mat4 proj;
    glm::mat4 viewproj;
    glm::vec4 ambientColor;
    glm::vec4 sunlightDirection; // w for sun power
    glm::vec4 sunlightColor;
};

class VulkanEngine
{
public:
    bool Init();
    bool Update();
    bool UnInit();

protected:
    void ImmediateSubmit(std::function<void(VkCommandBuffer cmd)>&& function);
    void RecordCommandBufferForDrawTestTriangle(VkCommandBuffer command_buffer, unsigned image_index);
    void RecordCommandBufferForDynamicRendering(VkCommandBuffer command_buffer);
    void DrawFrame();
    void CreateSwapChainAndRelativeResource();
    void CleanupSwapChain();
    void RecreateSwapChain();
    void InitMeshBuffer();
    void InitGraphicsPipeline();
    void InitComputePipeline();
    void TransitionImage(VkCommandBuffer cmd, VkImage image, VkImageLayout old_layout, VkImageLayout new_layout);
    
    AllocatedBuffer AllocateBuffer(size_t allocate_size, VkBufferUsageFlags usage_flags, VmaMemoryUsage memory_usage);
    void DestroyBuffer(const AllocatedBuffer& buffer);
    GPUMeshBuffers UploadMesh(std::span<uint32_t> indices, std::span<Vertex> vertices);
    
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
    VkPipelineLayout graphics_pipeline_layout {VK_NULL_HANDLE};
    VkRenderPass render_pass {VK_NULL_HANDLE};
    VkPipeline graphics_pipeline {VK_NULL_HANDLE};
    VkPipeline compute_pipeline {VK_NULL_HANDLE};
    std::vector<VkFramebuffer> swap_chain_frame_buffers;
    VkCommandPool command_pool {VK_NULL_HANDLE};
    std::vector<VkCommandBuffer> command_buffers;
    VkCommandBufferBeginInfo begin_info {};

    std::vector<VkSemaphore> image_available_semaphores;
    std::vector<VkSemaphore> render_finish_semaphores;
    std::vector<VkFence> in_flight_fences;
    std::vector<DescriptorAllocatorGrowable> m_frame_descriptors;
    std::vector<AllocatedBuffer> per_frame_scene_buffers{};
    
    unsigned current_frame_clipped {0};
    unsigned current_frame_real {0};
    bool window_resized {false};
    bool run_compute_test {true};
    bool run_graphics_test {true};
    bool init_render_pass {false};

    VmaAllocator vma_allocator {};
    AllocatedImage draw_image {};
    VkExtent2D draw_extent {};

    DescriptorAllocator globalDescriptorAllocator{};
    VkDescriptorSet _drawImageDescriptors{};
    VkDescriptorSetLayout _drawImageDescriptorLayout{};
    
    VkPipelineLayout _gradientPipelineLayout{};
    
    // immediate submit structures
    VkFence _immFence{VK_NULL_HANDLE};
    VkCommandBuffer _immCommandBuffer{VK_NULL_HANDLE};
    VkCommandPool _immCommandPool{VK_NULL_HANDLE};

    GPUMeshBuffers mesh_buffers{};
    GPUSceneData sceneData{};
    
    VkDescriptorSetLayout _gpuSceneDataDescriptorLayout{VK_NULL_HANDLE};
};
