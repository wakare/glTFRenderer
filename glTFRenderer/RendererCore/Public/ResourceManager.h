#pragma once
#include "RenderInterfaceCommon.h"

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

class ResourceManager
{
public:
    bool InitResourceManager(const RendererInterface::RenderDeviceDesc& desc);
    
protected:
    std::shared_ptr<IRHIFactory> m_factory;
    std::shared_ptr<IRHIDevice> m_device;
    std::shared_ptr<IRHICommandQueue> m_command_queue;
    std::shared_ptr<IRHISwapChain> m_swap_chain;
    std::shared_ptr<IRHIMemoryManager> m_memory_manager;
    
    std::vector<std::shared_ptr<IRHICommandAllocator>> m_command_allocators;
    std::vector<std::shared_ptr<IRHICommandList>> m_command_lists;
    std::vector<bool> m_command_list_record_state;

    std::shared_ptr<IRHIRenderTargetManager> m_render_target_manager;
    std::vector<std::shared_ptr<IRHITextureDescriptorAllocation>> m_swapchain_RTs;

    std::shared_ptr<glTFRenderMaterialManager> m_material_manager;
    std::shared_ptr<glTFRenderMeshManager> m_mesh_manager;
    
    std::shared_ptr<IRHIPipelineStateObject> m_current_pass_pso;
    //std::vector<glTFRenderResourceFrameManager> m_frame_resource_managers;
};
