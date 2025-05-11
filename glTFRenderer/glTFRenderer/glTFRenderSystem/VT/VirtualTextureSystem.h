#pragma once
#include <memory>

#include "RendererCommon.h"
#include "SVTPageStreamer.h"
#include "VTPageTable.h"
#include "VTTextureTypes.h"
#include "glTFRenderPass/glTFComputePass/glTFComputePassClearUAV.h"
#include "glTFRenderSystem/RenderSystemBase.h"

class glTFGraphicsPassMeshVTFeedbackBase;
class glTFComputePassVTFetchCS;
class glTFRenderPassBase;

class VirtualTextureSystem : public RenderSystemBase
{
public:
    enum
    {
        VT_PHYSICAL_TEXTURE_SIZE = 2048,
        VT_PAGE_SIZE = 32,
        VT_PHYSICAL_TEXTURE_BORDER = 1,
        VT_FEEDBACK_TEXTURE_SCALE_SIZE = 4,
        VT_PAGE_PROCESS_COUNT_PER_FRAME = 256, 
    };
    
    DECLARE_NON_COPYABLE_AND_DEFAULT_CTOR_VDTOR(VirtualTextureSystem)

    virtual bool InitRenderSystem(glTFRenderResourceManager& resource_manager) override;
    virtual void SetupPass(glTFRenderResourceManager& resource_manager, glTFRenderPassManager& pass_manager, const glTFSceneGraph& scene_graph) override;
    virtual void ShutdownRenderSystem() override;
    virtual void TickRenderSystem(glTFRenderResourceManager& resource_manager) override;

    bool HasTexture(std::shared_ptr<VTLogicalTextureBase> texture) const;
    bool RegisterTexture(std::shared_ptr<VTLogicalTextureBase> texture);
    bool UpdateRenderResource(glTFRenderResourceManager& resource_manager);

    const std::map<int, std::shared_ptr<VTLogicalTextureBase>>& GetLogicalTextureInfos() const;
    VTLogicalTextureBase& GetLogicalTextureInfo(unsigned virtual_texture_id) const;
    std::shared_ptr<VTPhysicalTexture> GetSVTPhysicalTexture() const;
    std::shared_ptr<VTPhysicalTexture> GetRVTPhysicalTexture() const;

    static std::pair<unsigned, unsigned> GetVTFeedbackTextureSize(unsigned width, unsigned height);
    unsigned GetNextValidVTIdAndInc();

    void RegisterVTFeedbackTextureRenderTargetId(RenderPassResourceTableId id);
    const std::set<RenderPassResourceTableId>& GetVTFeedbackTextureRenderTargetIds() const;
    
protected:
    VTPageType GetPageType(unsigned virtual_texture_id) const;
    void GatherPageRequest(std::vector<VTPage>& out_svt_pages, std::vector<VTPage>& out_rvt_pages);

    unsigned m_next_valid_vt_id{1};
    
    // one logical texture map one-page table
    std::map<int, std::shared_ptr<VTLogicalTextureBase>> m_logical_textures;
    std::vector<std::shared_ptr<VTPhysicalTexture>> m_physical_textures;
    
    std::shared_ptr<SVTPageStreamer> m_svt_page_streamer;

    std::set<RenderPassResourceTableId> m_feedback_render_target_ids;
    
    std::vector<std::shared_ptr<glTFComputePassVTFetchCS>> m_fetch_passes;
};

