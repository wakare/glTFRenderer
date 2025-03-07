#pragma once
#include <memory>

#include "RendererCommon.h"
#include "VTPageStreamer.h"
#include "VTPageTable.h"
#include "VTTextureTypes.h"
#include "glTFRenderSystem/RenderSystemBase.h"

class VirtualTextureSystem : public RenderSystemBase
{
public:
    enum
    {
        VT_PHYSICAL_TEXTURE_SIZE = 2048,
        VT_PAGE_SIZE = 64,
    };
    
    DECLARE_NON_COPYABLE_AND_DEFAULT_CTOR_VDTOR(VirtualTextureSystem)

    virtual bool InitRenderSystem() override;
    virtual void ShutdownRenderSystem() override;
    virtual void TickRenderSystem() override;

    bool RegisterTexture(std::shared_ptr<VTLogicalTexture> texture);
    
protected:
    void DrawFeedBackPass();
    void GatherPageRequest(std::vector<VTPage>& out_pages);
    
    // one logical texture map one page table
    std::map<int, std::pair<std::shared_ptr<VTLogicalTexture>, std::shared_ptr<VTPageTable>>> m_logical_texture_infos;
    std::shared_ptr<VTPageStreamer> m_page_streamer;
    std::shared_ptr<VTPhysicalTexture> m_physical_texture;
};
