#pragma once
#include "Renderer.h"

enum class RHIPipelineType;
enum class RHIDataFormat;
class IRHIRenderTarget;
class IRHIShader;
class IRHITextureAllocation;
class IRHITexture;
class glTFRenderResourceFrameManager;
class IRHIPipelineStateObject;
class glTFRenderMeshManager;
class glTFRenderMaterialManager;
class IRHITextureDescriptorAllocation;
class IRHIRenderTargetManager;
class IRHICommandList;
class IRHICommandAllocator;
class IRHIMemoryManager;
class IRHISwapChain;
class IRHICommandQueue;
class IRHIDevice;
class IRHIFactory;

namespace RendererInterfaceRHIConverter
{
    RHIDataFormat ConvertToRHIFormat(RendererInterface::PixelFormat format);
    RHIPipelineType ConvertToRHIPipelineType(RendererInterface::RenderPassType type);
}

class ResourceManager
{
public:
    bool InitResourceManager(const RendererInterface::RenderDeviceDesc& desc);

    RendererInterface::ShaderHandle CreateShader(const RendererInterface::ShaderDesc& shader_desc);
    RendererInterface::RenderTargetHandle CreateRenderTarget(const RendererInterface::RenderTargetDesc& desc);

    unsigned GetCurrentBackBufferIndex() const;
    
    IRHIDevice& GetDevice();
    IRHISwapChain& GetSwapChain();
    IRHIMemoryManager& GetMemoryManager();

    IRHICommandList& GetCommandListForRecordPassCommand(RendererInterface::RenderPassHandle render_pass_handle);

    IRHICommandQueue& GetCommandQueue();

    IRHITextureDescriptorAllocation& GetCurrentSwapchainRT();
    
protected:
    RendererInterface::RenderDeviceDesc m_device_desc{};
    
    std::shared_ptr<IRHIFactory> m_factory;
    std::shared_ptr<IRHIDevice> m_device;
    std::shared_ptr<IRHICommandQueue> m_command_queue;
    std::shared_ptr<IRHISwapChain> m_swap_chain;
    std::shared_ptr<IRHIMemoryManager> m_memory_manager;
    
    std::vector<std::shared_ptr<IRHICommandAllocator>> m_command_allocators;
    std::vector<std::shared_ptr<IRHICommandList>> m_command_lists;

    std::shared_ptr<IRHIRenderTargetManager> m_render_target_manager;
    std::vector<std::shared_ptr<IRHITextureDescriptorAllocation>> m_swapchain_RTs;

    std::shared_ptr<glTFRenderMaterialManager> m_material_manager;
    std::shared_ptr<glTFRenderMeshManager> m_mesh_manager;
    
    //std::vector<glTFRenderResourceFrameManager> m_frame_resource_managers;

    std::map<RendererInterface::ShaderHandle, std::shared_ptr<IRHIShader>> m_shaders;
    std::map<RendererInterface::RenderTargetHandle, std::shared_ptr<IRHITextureDescriptorAllocation>> m_render_targets;
};
