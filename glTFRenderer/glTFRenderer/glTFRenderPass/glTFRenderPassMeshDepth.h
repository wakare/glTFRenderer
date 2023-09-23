#pragma once
#include "glTFGraphicsPassMeshBase.h"

class glTFRenderPassMeshDepth : public glTFGraphicsPassMeshBase
{
public:
    virtual const char* PassName() override {return "MeshPassOpaque"; }
    virtual bool SetupPipelineStateObject(glTFRenderResourceManager& resource_manager) override;
    bool PreRenderPass(glTFRenderResourceManager& resource_manager) override;
    bool PostRenderPass(glTFRenderResourceManager& resource_manager) override;
protected:
    virtual size_t GetMainDescriptorHeapSize() override;
};
