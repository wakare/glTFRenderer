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
    enum
    {
        MeshOpaquePass_RootParameter_SceneView = 0,
        MeshOpaquePass_RootParameter_SceneMesh = 1,
        MeshOpaquePass_RootParameter_MeshMaterialTexSRV = 2,
        MeshOpaquePass_RootParameter_Num,
    };
    
public:
    virtual const char* PassName() override {return "MeshPassOpaque"; }
    virtual bool InitPass(glTFRenderResourceManager& resourceManager) override;
    virtual bool RenderPass(glTFRenderResourceManager& resourceManager) override;

    virtual bool ProcessMaterial(glTFRenderResourceManager& resourceManager,const glTFMaterialBase& material) override; 
    
protected:
    virtual size_t GetMainDescriptorHeapSize() override;
    virtual bool SetupRootSignature(glTFRenderResourceManager& resourceManager) override;
    virtual bool SetupPipelineStateObject(glTFRenderResourceManager& resourceManager) override;
    
    std::vector<RHIPipelineInputLayout> GetVertexInputLayout() override;
    std::vector<RHIPipelineInputLayout> ResolveVertexInputLayout(const glTFScenePrimitive& primitive);
    
    std::map<glTFUniqueID, std::unique_ptr<glTFAlbedoMaterialTextureResource>> m_materialTextures;
};
 