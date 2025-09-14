#pragma once
#include "Renderer.h"
#include "RHIInterface/IRHIRootSignatureHelper.h"

class ResourceManager;
class IRHIPipelineStateObject;
class IRHIDescriptorUpdater;

class RenderPass
{
public:
    RenderPass(RendererInterface::RenderPassDesc desc);

    bool InitRenderPass(ResourceManager& resource_manager);

    IRHIPipelineStateObject& GetPipelineStateObject();
    IRHIRootSignature& GetRootSignature();

    RendererInterface::RenderPassType GetRenderPassType() const;
    
protected:
    RendererInterface::RenderPassDesc m_desc;
    
    std::shared_ptr<IRHIRootSignature> m_root_signature;
    
    std::shared_ptr<IRHIDescriptorUpdater> m_descriptor_updater;
    std::shared_ptr<IRHIPipelineStateObject> m_pipeline_state_object;
};
