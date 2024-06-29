#pragma once
#include <memory>
#include <vector>

#include "RendererCommon.h"
#include "glTFRenderMeshManager.h"
#include "glTFRenderResourceFrameManager.h"
#include "glTFApp/glTFRadiosityRenderer.h"
#include "glTFRHI/RHIInterface/IRHICommandList.h"
#include "glTFRHI/RHIInterface/IRHIDevice.h"
#include "glTFRHI/RHIInterface/IRHIFence.h"
#include "glTFRHI/RHIInterface/IRHIRenderTarget.h"
#include "glTFRHI/RHIInterface/IRHISwapChain.h"
#include "glTFRHI/RHIInterface/IRHIDescriptorHeap.h"
#include "glTFRHI/RHIInterface/IRHIPipelineStateObject.h"

class IRHIFrameBuffer;
class glTFWindow;
class glTFRenderMaterialManager;

// Hold all rhi resource
class glTFRenderResourceManager
{
public:
    glTFRenderResourceManager();
    
    bool InitResourceManager(unsigned width, unsigned height, HWND handle);
    bool InitScene(const glTFSceneGraph& scene_graph);
    
    IRHIFactory& GetFactory();
    IRHIDevice& GetDevice();
    IRHISwapChain& GetSwapChain();
    static IRHICommandQueue& GetCommandQueue();
    IRHICommandList& GetCommandListForRecord();
    void CloseCommandListAndExecute(bool wait);

    void WaitAllFrameFinish() const;
    void ResetCommandAllocator();
    
    IRHIRenderTargetManager& GetRenderTargetManager();
    
    IRHICommandAllocator& GetCurrentFrameCommandAllocator();
    
    IRHIRenderTarget& GetCurrentFrameSwapChainRT();
    IRHIRenderTarget& GetDepthRT();

    static unsigned GetCurrentBackBufferIndex() { return m_swap_chain->GetCurrentBackBufferIndex(); }

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

    glTFRadiosityRenderer& GetRadiosityRenderer();
    const glTFRadiosityRenderer& GetRadiosityRenderer() const;
    
private:
    std::shared_ptr<glTFRadiosityRenderer> m_radiosity_renderer;
    
    static std::shared_ptr<IRHIFactory> m_factory;
    static std::shared_ptr<IRHIDevice> m_device;
    static std::shared_ptr<IRHICommandQueue> m_command_queue;
    static std::shared_ptr<IRHISwapChain> m_swap_chain;
    
    std::vector<std::shared_ptr<IRHICommandAllocator>> m_command_allocators;
    std::vector<std::shared_ptr<IRHICommandList>> m_command_lists;
    std::vector<bool> m_command_list_record_state;
    
    std::shared_ptr<IRHIRenderTargetManager> m_render_target_manager;
    std::vector<std::shared_ptr<IRHIRenderTarget>> m_swapchain_RTs;
    std::shared_ptr<IRHIRenderTarget> m_depth_texture;

    std::shared_ptr<glTFRenderMaterialManager> m_material_manager;
    std::shared_ptr<glTFRenderMeshManager> m_mesh_manager;
    
    std::shared_ptr<IRHIPipelineStateObject> m_current_pass_pso;
    std::vector<glTFRenderResourceFrameManager> m_frame_resource_managers;
    
    std::shared_ptr<glTFRenderResourceUtils::GBufferSignatureAllocations> m_GBuffer_allocations;
};
