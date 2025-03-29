#pragma once
#include <memory>
#include <vector>

#include "glTFRenderMeshManager.h"
#include "glTFRenderPassCommon.h"
#include "glTFRenderResourceFrameManager.h"
#include "RenderGraphNodeUtil.h"
#include "glTFScene/glTFSceneView.h"

class glTFSceneView;
class RenderSystemBase;
class glTFSceneGraph;
class IRHIFrameBuffer;
class glTFWindow;
class glTFRenderMaterialManager;
class IRHIFactory;
class IRHISwapChain;
class IRHICommandQueue;
class IRHIPipelineStateObject;

class glTFPerFrameRenderResourceData
{
public:
    //DECLARE_NON_COPYABLE_AND_DEFAULT_CTOR_VDTOR(glTFPerFrameRenderResourceData)
    bool InitSceneViewData(IRHIMemoryManager& memory_manager, IRHIDevice& device);
    bool UpdateSceneViewData(IRHIMemoryManager& memory_manager, IRHIDevice& device, const ConstantBufferSceneView& scene_view);
    const ConstantBufferSceneView& GetSceneView() const;
    std::shared_ptr<IRHIBufferAllocation> GetSceneViewBufferAllocation() const;

    bool InitShadowmapSceneViewData(IRHIMemoryManager& memory_manager, IRHIDevice& device, unsigned light_id);
    bool UpdateShadowmapSceneViewData(IRHIMemoryManager& memory_manager, IRHIDevice& device, unsigned light_id, const ConstantBufferSceneView& scene_view);
    const ConstantBufferSceneView& GetShadowmapSceneView(unsigned light_id) const;
    const std::map<unsigned, ConstantBufferSceneView>& GetShadowmapSceneViews() const;
    std::shared_ptr<IRHIBufferAllocation> GetShadowmapViewBufferAllocation(unsigned light_id) const;
    
protected:
    ConstantBufferSceneView m_scene_view;
    std::shared_ptr<IRHIBufferAllocation> m_scene_view_buffer;

    // Directional light shadowmap views
    std::map<unsigned, ConstantBufferSceneView> m_shadowmap_view_data;
    std::map<unsigned, std::shared_ptr<IRHIBufferAllocation>> m_shadowmap_view_buffers;
};

// Hold all rhi resource
class glTFRenderResourceManager final
{
public:
    DECLARE_NON_COPYABLE_AND_DEFAULT_CTOR_VDTOR(glTFRenderResourceManager)
    
    bool InitResourceManager(unsigned width, unsigned height, HWND handle);
    bool InitScene(const glTFSceneGraph& scene_graph);
    bool InitMemoryManager();
    bool InitRenderSystems();
    
    IRHIFactory& GetFactory() const;
    IRHIDevice& GetDevice() const;
    IRHISwapChain& GetSwapChain() const;
    IRHICommandQueue& GetCommandQueue() const;

    bool IsMemoryManagerValid() const;
    IRHIMemoryManager& GetMemoryManager() const;
    
    unsigned GetCurrentBackBufferIndex() const;
    
    IRHICommandList& GetCommandListForRecord();
    void CloseCurrentCommandListAndExecute(const RHIExecuteCommandListContext& context, bool wait);

    void WaitPresentFinished();
    void WaitLastFrameFinish() const;
    void WaitAllFrameFinish() const;
    void ResetCommandAllocator();
    void WaitAndClean();
    
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

    std::vector<glTFPerFrameRenderResourceData>& GetPerFrameRenderResourceData();
    const std::vector<glTFPerFrameRenderResourceData>& GetPerFrameRenderResourceData() const;

    RenderGraphNodeUtil::RenderGraphNodeFinalOutput& GetFinalOutput();

    void AddRenderSystem(std::shared_ptr<RenderSystemBase> render_system);

    template<typename system_type>
    std::shared_ptr<system_type> GetRenderSystem() const;

    std::vector<std::shared_ptr<RenderSystemBase>> GetRenderSystems() const;

    void TickFrame();
    void TickSceneUpdating(const glTFSceneView& scene_view, glTFRenderResourceManager& resource_manager, const glTFSceneGraph& scene_graph, size_t delta_time_ms);

    
private:
    //std::shared_ptr<glTFRadiosityRenderer> m_radiosity_renderer;
    
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
    std::vector<glTFRenderResourceFrameManager> m_frame_resource_managers;
    
    std::shared_ptr<glTFRenderResourceUtils::GBufferSignatureAllocations> m_gBuffer_allocations;

    std::map<RenderPassResourceTableId, std::vector<std::shared_ptr<IRHITexture>>> m_export_texture_map;
    std::map<RenderPassResourceTableId, std::vector<std::shared_ptr<IRHITextureAllocation>>> m_export_texture_allocation_map;
    std::map<RenderPassResourceTableId, std::vector<std::shared_ptr<IRHITextureDescriptorAllocation>>> m_export_texture_descriptor_map;

    std::vector<glTFPerFrameRenderResourceData> m_per_frame_render_resource_data;

    RenderGraphNodeUtil::RenderGraphNodeFinalOutput m_final_output;

    std::vector<std::shared_ptr<RenderSystemBase>> m_render_systems;
};

template <typename system_type>
std::shared_ptr<system_type> glTFRenderResourceManager::GetRenderSystem() const
{
    for (auto& render_system : m_render_systems)
    {
        if (std::shared_ptr<system_type> pointer = dynamic_pointer_cast<system_type>(render_system))
        {
            return pointer;
        }
    }
    
    return nullptr;
}
