#pragma once
#include <memory>

#include "RendererCommon.h"
#include "VTPageStreamer.h"
#include "VTPageTable.h"
#include "VTTextureTypes.h"
#include "glTFRenderPass/glTFComputePass/glTFComputePassClearUAV.h"
#include "glTFRenderSystem/RenderSystemBase.h"

class glTFGraphicsPassMeshVTFeedback;
class glTFComputePassVTFetchCS;
class glTFRenderPassBase;

class VirtualTextureSystem : public RenderSystemBase
{
public:
    enum
    {
        VT_PHYSICAL_TEXTURE_SIZE = 512,
        VT_PAGE_SIZE = 64,
        VT_PHYSICAL_TEXTURE_BORDER = 1,
        VT_FEEDBACK_TEXTURE_SCALE_SIZE = 32,
        VT_PAGE_PROCESS_COUNT_PER_FRAME = 5, 
    };
    
    DECLARE_NON_COPYABLE_AND_DEFAULT_CTOR_VDTOR(VirtualTextureSystem)

    virtual bool InitRenderSystem(glTFRenderResourceManager& resource_manager) override;
    virtual void SetupPass(glTFRenderResourceManager& resource_manager, glTFRenderPassManager& pass_manager, const glTFSceneGraph& scene_graph) override;
    virtual void ShutdownRenderSystem() override;
    virtual void TickRenderSystem(glTFRenderResourceManager& resource_manager) override;

    bool HasTexture(std::shared_ptr<VTLogicalTexture> texture) const;
    bool RegisterTexture(std::shared_ptr<VTLogicalTexture> texture);
    bool UpdateRenderResource(glTFRenderResourceManager& resource_manager);

    const std::map<int, std::shared_ptr<VTLogicalTexture>>& GetLogicalTextureInfos() const;
    VTLogicalTexture& GetLogicalTextureInfo(unsigned virtual_texture_id) const;
    std::shared_ptr<VTPhysicalTexture> GetSVTPhysicalTexture() const;
    std::shared_ptr<VTPhysicalTexture> GetRVTPhysicalTexture() const;

    static std::pair<unsigned, unsigned> GetVTFeedbackTextureSize(const VTLogicalTexture& logical_texture);
    unsigned GetAvailableVTIdAndInc();
    
protected:
    VTPageType GetPageType(unsigned virtual_texture_id) const;
    void InitFeedBackPass();
    void GatherPageRequest(std::vector<VTPage>& out_svt_pages);
    
    // one logical texture map one page table
    std::map<int, std::shared_ptr<VTLogicalTexture>> m_logical_texture_infos;
    std::map<int, std::pair<std::shared_ptr<glTFGraphicsPassMeshVTFeedback>, std::shared_ptr<glTFComputePassVTFetchCS>>> m_logical_texture_feedback_passes;

    unsigned m_virtual_texture_id{1};
    
    std::shared_ptr<VTPageStreamer> m_page_streamer;
    std::vector<std::shared_ptr<VTPhysicalTexture>> m_physical_virtual_textures;

    std::set<VTPage::HashType> m_loaded_page_hashes;
};
