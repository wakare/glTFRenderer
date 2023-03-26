#pragma once
#include "glTFRenderPassMeshBase.h"
#include "../glTFRHI/RHIInterface/IRHITexture.h"

class glTFRenderPassMeshOpaque : public glTFRenderPassMeshBase
{
public:
    virtual const char* PassName() override {return "MeshPassOpaque"; }
    virtual bool InitPass(glTFRenderResourceManager& resourceManager) override;
    virtual bool RenderPass(glTFRenderResourceManager& resourceManager) override;

protected:
    size_t GetMainDescriptorHeapSize() override;
    std::vector<RHIPipelineInputLayout> GetVertexInputLayout() override;

    std::vector<RHIPipelineInputLayout> ResolveVertexInputLayout(const glTFScenePrimitive& primitive);
    
    std::shared_ptr<IRHITexture> m_textureBuffer;
    RHICPUDescriptorHandle m_textureSRVHandle = 0;
};
