#pragma once
#include <memory>

#include "RendererCommon.h"
#include "VTPageStreamer.h"
#include "VTPageTable.h"
#include "VTTextureTypes.h"
#include "glTFRenderPass/glTFComputePass/glTFComputePassClearUAV.h"
#include "glTFRenderSystem/RenderSystemBase.h"

class glTFComputePassVTFetchCS;
class glTFRenderPassBase;

class VirtualTextureSystem : public RenderSystemBase
{
public:
    enum
    {
        VT_PHYSICAL_TEXTURE_SIZE = 1024,
        VT_PAGE_SIZE = 64,
        VT_PHYSICAL_TEXTURE_BORDER = 1,
        VT_FEEDBACK_TEXTURE_SCALE_SIZE = 8,
    };
    
    DECLARE_NON_COPYABLE_AND_DEFAULT_CTOR_VDTOR(VirtualTextureSystem)

    virtual bool InitRenderSystem(glTFRenderResourceManager& resource_manager) override;
    virtual void SetupPass(glTFRenderPassManager& pass_manager, const glTFSceneGraph& scene_graph) override;
    virtual void ShutdownRenderSystem() override;
    virtual void TickRenderSystem(glTFRenderResourceManager& resource_manager) override;

    bool HasTexture(std::shared_ptr<VTLogicalTexture> texture) const;
    bool RegisterTexture(std::shared_ptr<VTLogicalTexture> texture);
    bool UpdateRenderResource(glTFRenderResourceManager& resource_manager);

    const std::map<int, std::pair<std::shared_ptr<VTLogicalTexture>, std::shared_ptr<VTPageTable>>>& GetLogicalTextureInfos() const;
    std::shared_ptr<VTPhysicalTexture> GetPhysicalTexture() const;

    static std::pair<unsigned, unsigned> GetVTFeedbackTextureSize(glTFRenderResourceManager& resource_manager);
    
protected:
    void InitFeedBackPass();
    void GatherPageRequest(std::vector<VTPage>& out_pages);
    
    // one logical texture map one page table
    std::map<int, std::pair<std::shared_ptr<VTLogicalTexture>, std::shared_ptr<VTPageTable>>> m_logical_texture_infos;
    std::shared_ptr<VTPageStreamer> m_page_streamer;
    std::shared_ptr<VTPhysicalTexture> m_physical_texture;

    std::shared_ptr<glTFRenderPassBase> m_feedback_pass;
    std::shared_ptr<glTFComputePassVTFetchCS> m_fetch_feedback_pass;
};
