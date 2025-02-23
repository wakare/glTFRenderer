#pragma once
#include "glTFRenderPassCommon.h"

class IRHITexture;

namespace RenderGraphNodeUtil
{
    class RenderGraphNodeFinalOutput
    {
    public:
        // Copy this output texture to back buffer
        std::shared_ptr<IRHITexture> final_color_output;
    };
    
    class RenderGraphNode
    {
    public:
        DECLARE_NON_COPYABLE_AND_DEFAULT_CTOR_VDTOR(RenderGraphNode)
        
        std::shared_ptr<IRHITexture> GetResourceTexture(RenderPassResourceTableId id) const;
        std::shared_ptr<IRHITextureDescriptorAllocation> GetResourceDescriptor(RenderPassResourceTableId id) const;
        
        void AddImportTextureResource(RenderPassResourceTableId id, const RHITextureDesc& desc, const RHITextureDescriptorDesc& descriptor_desc);
        void AddExportTextureResource(RenderPassResourceTableId id, const RHITextureDesc& texture_desc, const RHITextureDescriptorDesc& descriptor_desc);
        
        // render pass implement this method for requiring global shader resource location
        virtual bool InitResourceTable(glTFRenderResourceManager& resource_manager);
        bool ExportResourceLocation(glTFRenderResourceManager& resource_manager);
        bool ImportResourceLocation(glTFRenderResourceManager& resource_manager);
    
        const RenderPassResourceTable& GetResourceTable() const;
        virtual bool ModifyFinalOutput(RenderGraphNodeFinalOutput& final_output) {return true;}

        void UpdateFrameIndex(const glTFRenderResourceManager& resource_manager);
        void AddFinalOutputCandidate(RenderPassResourceTableId id);
        
    protected:
        unsigned m_current_frame_index = 0;

        int m_final_output_index = 0;
        std::vector<RenderPassResourceTableId> m_final_output_candidates;
        
        RenderPassResourceTable m_resource_table;
        RenderPassResourceLocation m_resource_location;
    };

    
};
