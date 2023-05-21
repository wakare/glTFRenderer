#pragma once
#include "glTFRenderPassMeshBase.h"
#include "../glTFMaterial/glTFMaterialTexture.h"
#include "../glTFRHI/RHIInterface/IRHITexture.h"

class glTFAlbedoMaterialTextureResource
{
public:
    glTFAlbedoMaterialTextureResource(const glTFMaterialTexture& material)
        : m_sourceTexture(material)
        , m_textureBuffer(nullptr)
        , m_textureSRVHandle(0)
    {
        
    }

    bool Init(glTFRenderResourceManager& resourceManager, IRHIDescriptorHeap& descriptorHeap);
    RHICPUDescriptorHandle GetTextureSRVHandle() const;
    
    
private:
    const glTFMaterialTexture& m_sourceTexture;
    std::shared_ptr<IRHITexture> m_textureBuffer;
    RHICPUDescriptorHandle m_textureSRVHandle; 
};

class glTFRenderPassMeshOpaque : public glTFRenderPassMeshBase
{
public:
    virtual const char* PassName() override {return "MeshPassOpaque"; }
    virtual bool InitPass(glTFRenderResourceManager& resourceManager) override;
    virtual bool RenderPass(glTFRenderResourceManager& resourceManager) override;

    virtual bool ProcessMaterial(glTFRenderResourceManager& resourceManager,const glTFMaterialBase& material) override; 
    
protected:
    size_t GetMainDescriptorHeapSize();
    virtual bool SetupMainDescriptorHeap(glTFRenderResourceManager& resourceManager) override;
    virtual bool SetupRootSignature(glTFRenderResourceManager& resourceManager) override;
    virtual bool SetupPipelineStateObject(glTFRenderResourceManager& resourceManager) override;
    
    std::vector<RHIPipelineInputLayout> GetVertexInputLayout() override;
    std::vector<RHIPipelineInputLayout> ResolveVertexInputLayout(const glTFScenePrimitive& primitive);
    
    std::map<glTFUniqueID, std::unique_ptr<glTFAlbedoMaterialTextureResource>> m_materialTextures;
};
 