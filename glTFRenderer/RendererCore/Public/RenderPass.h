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

    const RootSignatureAllocation& GetRootSignatureAllocation(const std::string& name) const;

    IRHIDescriptorUpdater& GetDescriptorUpdater();
    
protected:
    RendererInterface::RenderPassDesc m_desc;
    
    IRHIRootSignatureHelper m_root_signature_helper;
    
    std::shared_ptr<IRHIDescriptorUpdater> m_descriptor_updater;
    std::shared_ptr<IRHIPipelineStateObject> m_pipeline_state_object;

    std::map<std::string, RootSignatureAllocation> m_shader_parameter_mapping;
};
