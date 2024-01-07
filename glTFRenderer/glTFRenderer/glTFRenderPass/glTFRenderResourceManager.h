#pragma once
#include <memory>
#include <vector>

#include "glTFUtils/glTFUtils.h"
#include "glTFRenderMeshManager.h"
#include "glTFRenderResourceFrameManager.h"
#include "glTFRHI/RHIInterface/IRHICommandList.h"
#include "glTFRHI/RHIInterface/IRHIDevice.h"
#include "glTFRHI/RHIInterface/IRHIFence.h"
#include "glTFRHI/RHIInterface/IRHIRenderTarget.h"
#include "glTFRHI/RHIInterface/IRHISwapChain.h"
#include "glTFRHI/RHIInterface/IRHIDescriptorHeap.h"
#include "glTFRHI/RHIInterface/IRHIPipelineStateObject.h"

class glTFWindow;
class glTFRenderMaterialManager;

// Hold all rhi resource
class glTFRenderResourceManager
{
public:
    glTFRenderResourceManager();
    
    bool InitResourceManager(unsigned width, unsigned height, HWND handle);

    IRHIFactory& GetFactory();
    IRHIDevice& GetDevice();
    IRHISwapChain& GetSwapchain();
    IRHICommandQueue& GetCommandQueue();
    IRHICommandList& GetCommandListForRecord();
    void CloseCommandListAndExecute(bool wait);

    void WaitLastFrameFinish();
    void ResetCommandAllocator();
    
    IRHIRenderTargetManager& GetRenderTargetManager();
    
    IRHICommandAllocator& GetCurrentFrameCommandAllocator();
    IRHIFence& GetCurrentFrameFence();
    IRHIRenderTarget& GetCurrentFrameSwapchainRT();
    IRHIRenderTarget& GetDepthRT();

    unsigned GetCurrentBackBufferIndex() const {return m_currentBackBufferIndex; }
    void UpdateCurrentBackBufferIndex() { m_currentBackBufferIndex = m_swapchain->GetCurrentBackBufferIndex(); }

	glTFRenderMaterialManager& GetMaterialManager();
    glTFRenderMeshManager& GetMeshManager();
    
    bool ApplyMaterial(IRHIDescriptorHeap& descriptor_heap, glTFUniqueID material_ID, unsigned slot_index, bool isGraphicsPipeline);
    bool TryProcessSceneObject(glTFRenderResourceManager& resource_manager, const glTFSceneObjectBase& object);
    
    void SetCurrentPSO(std::shared_ptr<IRHIPipelineStateObject> pso);

    const glTFRenderResourceFrameManager& GetCurrentFrameResourceManager() const;
    glTFRenderResourceFrameManager& GetCurrentFrameResourceManager();

    const glTFRenderResourceFrameManager& GetFrameResourceManagerByIndex(unsigned index) const;
    glTFRenderResourceFrameManager& GetFrameResourceManagerByIndex(unsigned index);
    
    static unsigned GetBackBufferCount();
    glTFRenderResourceUtils::GBufferSignatureAllocations& GetGBufferAllocations();
    
private:
    std::shared_ptr<IRHIFactory> m_factory;
    std::shared_ptr<IRHIDevice> m_device;
    std::shared_ptr<IRHISwapChain> m_swapchain;
    std::shared_ptr<IRHICommandQueue> m_command_queue;
    std::vector<std::shared_ptr<IRHICommandAllocator>> m_command_allocators;
    std::vector<std::shared_ptr<IRHICommandList>> m_command_lists;
    std::vector<bool> m_command_list_record_state;
    
    std::vector<std::shared_ptr<IRHIFence>> m_fences;
    std::shared_ptr<IRHIRenderTargetManager> m_render_target_manager;
    std::vector<std::shared_ptr<IRHIRenderTarget>> m_swapchain_RTs;
    std::shared_ptr<IRHIRenderTarget> m_depth_texture;

    std::shared_ptr<glTFRenderMaterialManager> m_material_manager;
    std::shared_ptr<glTFRenderMeshManager> m_mesh_manager;
    
    std::shared_ptr<IRHIPipelineStateObject> m_current_pass_pso;
    std::vector<glTFRenderResourceFrameManager> m_frame_resource_managers;
    
    unsigned m_currentBackBufferIndex;

    std::shared_ptr<glTFRenderResourceUtils::GBufferSignatureAllocations> m_GBuffer_allocations;
};
