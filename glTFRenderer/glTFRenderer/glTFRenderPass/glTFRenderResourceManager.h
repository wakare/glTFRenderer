#pragma once
#include <memory>
#include <vector>

#include "glTFRenderMeshManager.h"
#include "glTFRenderPassCommon.h"
#include "glTFRenderResourceFrameManager.h"
#include "RenderGraphNodeUtil.h"

class glTFSceneGraph;
class IRHIFrameBuffer;
class glTFWindow;
class glTFRenderMaterialManager;
class IRHIFactory;
class IRHISwapChain;
class IRHICommandQueue;
class IRHIPipelineStateObject;

struct glTFPerFrameRenderResourceData
{
    std::shared_ptr<IRHIBufferAllocation> m_scene_view_buffer;
};

// Hold all rhi resource
class glTFRenderResourceManager
{
public:
    glTFRenderResourceManager();
    
    bool InitResourceManager(unsigned width, unsigned height, HWND handle);
    bool InitScene(const glTFSceneGraph& scene_graph);
    bool InitMemoryManager();

    static IRHIFactory& GetFactory();
    static IRHIDevice& GetDevice();
    static IRHISwapChain& GetSwapChain();
    static IRHICommandQueue& GetCommandQueue();
    IRHIMemoryAllocator& GetMemoryAllocator() const;
    IRHIMemoryManager& GetMemoryManager() const;
    
    static unsigned GetCurrentBackBufferIndex();
    
    IRHICommandList& GetCommandListForRecord();
    void CloseCurrentCommandListAndExecute(const RHIExecuteCommandListContext& context, bool wait);

    void WaitPresentFinished();
    void WaitLastFrameFinish() const;
    void WaitAllFrameFinish() const;
    void ResetCommandAllocator();
    
    IRHIRenderTargetManager& GetRenderTargetManager();
    
    IRHICommandAllocator& GetCurrentFrameCommandAllocator();

    IRHITexture& GetCurrentFrameSwapChainTexture();
    IRHITexture& GetDepthTextureRef();
    std::shared_ptr<IRHITexture> GetDepthTexture();
    
    IRHITextureDescriptorAllocation& GetCurrentFrameSwapChainRTV();
    IRHITextureDescriptorAllocation& GetDepthDSV();

	glTFRenderMaterialManager& GetMaterialManager();
    glTFRenderMeshManager& GetMeshManager();
    
    bool TryProcessSceneObject(glTFRenderResourceManager& resource_manager, const glTFSceneObjectBase& object);
    
    void SetCurrentPSO(std::shared_ptr<IRHIPipelineStateObject> pso);

    const glTFRenderResourceFrameManager& GetCurrentFrameResourceManager() const;
    glTFRenderResourceFrameManager& GetCurrentFrameResourceManager();

    const glTFRenderResourceFrameManager& GetFrameResourceManagerByIndex(unsigned index) const;
    glTFRenderResourceFrameManager& GetFrameResourceManagerByIndex(unsigned index);
    
    static unsigned GetBackBufferCount();
    glTFRenderResourceUtils::GBufferSignatureAllocations& GetGBufferAllocations();

    //glTFRadiosityRenderer& GetRadiosityRenderer();
    //const glTFRadiosityRenderer& GetRadiosityRenderer() const;

    // Allocate pass resource and track for export/import
    bool ExportResourceTexture(const RHITextureDesc& desc, RenderPassResourceTableId entry_id, std::vector<std::shared_ptr<IRHITexture>>& out_texture_allocation);
    bool ImportResourceTexture(const RHITextureDesc& desc, RenderPassResourceTableId entry_id, std::vector<std::shared_ptr<IRHITexture>>& out_texture_allocation);

    const std::vector<glTFPerFrameRenderResourceData>& GetPerFrameRenderResourceData() const;

    RenderGraphNodeUtil::RenderGraphNodeFinalOutput& GetFinalOutput();
    
private:
    //std::shared_ptr<glTFRadiosityRenderer> m_radiosity_renderer;
    
    static std::shared_ptr<IRHIFactory> m_factory;
    static std::shared_ptr<IRHIDevice> m_device;
    static std::shared_ptr<IRHICommandQueue> m_command_queue;
    static std::shared_ptr<IRHISwapChain> m_swap_chain;
    std::shared_ptr<IRHIMemoryAllocator> m_memory_allocator;
    std::shared_ptr<IRHIMemoryManager> m_memory_manager;
    
    std::vector<std::shared_ptr<IRHICommandAllocator>> m_command_allocators;
    std::vector<std::shared_ptr<IRHICommandList>> m_command_lists;
    std::vector<bool> m_command_list_record_state;

    std::shared_ptr<IRHIRenderTargetManager> m_render_target_manager;
    std::vector<std::shared_ptr<IRHITextureDescriptorAllocation>> m_swapchain_RTs;

    std::shared_ptr<glTFRenderMaterialManager> m_material_manager;
    std::shared_ptr<glTFRenderMeshManager> m_mesh_manager;
    
    std::shared_ptr<IRHIPipelineStateObject> m_current_pass_pso;
    std::vector<glTFRenderResourceFrameManager> m_frame_resource_managers;
    
    std::shared_ptr<glTFRenderResourceUtils::GBufferSignatureAllocations> m_gBuffer_allocations;

    std::map<RenderPassResourceTableId, std::vector<std::shared_ptr<IRHITexture>>> m_export_texture_map;
    std::map<RenderPassResourceTableId, std::vector<std::shared_ptr<IRHITextureAllocation>>> m_export_texture_allocation_map;
    std::map<RenderPassResourceTableId, std::vector<std::shared_ptr<IRHITextureDescriptorAllocation>>> m_export_texture_descriptor_map;

    std::vector<glTFPerFrameRenderResourceData> m_per_frame_render_resource_data;

    RenderGraphNodeUtil::RenderGraphNodeFinalOutput m_final_output;
};
