#pragma once
#include "glTFRenderPassCommon.h"

class IRHITexture;

namespace RenderGraphNodeUtil
{
    class RenderGraphNode
    {
    public:
        DECLARE_NON_COPYABLE_AND_DEFAULT_CTOR_VDTOR(RenderGraphNode)
        
        std::shared_ptr<IRHITexture> GetResourceTexture(RenderPassResourceTableId id) const;
        
        void AddImportTextureResource(const RHITextureDesc& desc, RenderPassResourceTableId id);
        void AddExportTextureResource(const RHITextureDesc& desc, RenderPassResourceTableId id);
        
        // render pass implement this method for requiring global shader resource location
        virtual bool InitResourceTable(glTFRenderResourceManager& resource_manager); 
        bool ExportResourceLocation(glTFRenderResourceManager& resource_manager);
        bool ImportResourceLocation(glTFRenderResourceManager& resource_manager);
    
        const RenderPassResourceTable& GetResourceTable() const;
        
    protected:
        RenderPassResourceTable m_resource_table;
        RenderPassResourceLocation m_resource_location;
    };

    
};
