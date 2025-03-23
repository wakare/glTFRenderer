#pragma once
#include "glTFGraphicsPassMeshBase.h"

struct ShadowmapPassConfig
{
    int light_id;
};

class glTFGraphicsPassMeshShadowDepth : public glTFGraphicsPassMeshBase
{
public:
    glTFGraphicsPassMeshShadowDepth(const ShadowmapPassConfig& config);
    DECLARE_NON_COPYABLE_AND_VDTOR(glTFGraphicsPassMeshShadowDepth)

    virtual const char* PassName() override {return "ShadowmapDepthPass"; }
    
protected:
    virtual bool InitRenderInterface(glTFRenderResourceManager& resource_manager) override;
    virtual bool InitResourceTable(glTFRenderResourceManager& resource_manager) override;
    virtual bool PreRenderPass(glTFRenderResourceManager& resource_manager) override;
    virtual bool SetupPipelineStateObject(glTFRenderResourceManager& resource_manager) override;

    ShadowmapPassConfig m_config;
};
