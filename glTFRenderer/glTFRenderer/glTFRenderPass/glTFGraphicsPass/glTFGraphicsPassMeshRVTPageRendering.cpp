#include "glTFGraphicsPassMeshRVTPageRendering.h"

#include "glTFRenderPass/glTFRenderInterface/glTFRenderInterfaceViewBase.h"
#include "glTFRenderSystem/VT/VirtualTextureSystem.h"
#include "RHIInterface/IRHIPipelineStateObject.h"
#include "RHIInterface/IRHIRenderTargetManager.h"

struct RVTOutputTileOffset
{
    inline static std::string Name = "RVT_OUTPUT_FILE_OFFSET_REGISTER_INDEX";
    
    unsigned offset_x;
    unsigned offset_y;
    unsigned width;
    unsigned height;
};

bool glTFGraphicsPassMeshRVTPageRendering::InitPass(glTFRenderResourceManager& resource_manager)
{
    RETURN_IF_FALSE(glTFGraphicsPassMeshBase::InitPass(resource_manager))

    return true;
}

bool glTFGraphicsPassMeshRVTPageRendering::SetupRootSignature(glTFRenderResourceManager& resource_manager)
{
    RETURN_IF_FALSE(glTFGraphicsPassMeshBase::SetupRootSignature(resource_manager))

    return true;
}

void glTFGraphicsPassMeshRVTPageRendering::SetupNextPageRenderingInfo(glTFRenderResourceManager& resource_manager, const RVTPageRenderingInfo& page_rendering_info)
{
    m_page_rendering_info = page_rendering_info;
    m_rendering_enabled = m_page_rendering_info.physical_page_x >= 0 && m_page_rendering_info.physical_page_y >= 0;
}

bool glTFGraphicsPassMeshRVTPageRendering::InitRenderInterface(glTFRenderResourceManager& resource_manager)
{
    RETURN_IF_FALSE(glTFGraphicsPassMeshBase::InitRenderInterface(resource_manager))

    AddRenderInterface(std::make_shared<glTFRenderInterfaceSingleConstantBuffer<RVTOutputTileOffset>>());
    
    return true;
}

bool glTFGraphicsPassMeshRVTPageRendering::SetupPipelineStateObject(glTFRenderResourceManager& resource_manager)
{
    RETURN_IF_FALSE(glTFGraphicsPassMeshBase::SetupPipelineStateObject(resource_manager))
    
    
    return true;
}

bool glTFGraphicsPassMeshRVTPageRendering::PreRenderPass(glTFRenderResourceManager& resource_manager)
{
    RETURN_IF_FALSE(glTFGraphicsPassMeshBase::PreRenderPass(resource_manager))

    RVTOutputTileOffset offset;
    offset.offset_x = m_page_rendering_info.physical_page_x;
    offset.offset_y = m_page_rendering_info.physical_page_y;
    GetRenderInterface<glTFRenderInterfaceSingleConstantBuffer<RVTOutputTileOffset>>()->UploadBuffer(resource_manager, &offset, 0 ,sizeof(offset) );

    return true;
}

bool glTFGraphicsPassMeshRVTPageRendering::PostRenderPass(glTFRenderResourceManager& resource_manager)
{
    RETURN_IF_FALSE(glTFGraphicsPassMeshBase::PostRenderPass(resource_manager))

    m_page_rendering_info.physical_page_x = -1;
    m_page_rendering_info.physical_page_y = -1;
    
    m_rendering_enabled = false;
    
    return true;
}

RHIViewportDesc glTFGraphicsPassMeshRVTPageRendering::GetViewport(glTFRenderResourceManager& resource_manager) const
{
    RHIViewportDesc viewport_desc
        {
            static_cast<float>(m_page_rendering_info.physical_page_x), static_cast<float>(m_page_rendering_info.physical_page_y),
            static_cast<float>(m_page_rendering_info.physical_page_size), static_cast<float>(m_page_rendering_info.physical_page_size),
            0.0f, 1.0f
        };

    return viewport_desc;
}
