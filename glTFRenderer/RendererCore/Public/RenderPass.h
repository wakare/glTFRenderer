#pragma once
#include "Renderer.h"
#include "RHIInterface/IRHIRootSignatureHelper.h"

class ResourceManager;
class IRHIPipelineStateObject;
class IRHIDescriptorUpdater;
class IRHIDevice;
class IRHISwapChain;
class IRHIResource;
class IRHIShaderTable;

class RenderPass
{
public:
    struct DrawValidationResult
    {
        bool valid{true};
        std::vector<std::string> errors;
        std::vector<std::string> warnings;
    };

    RenderPass(RendererInterface::RenderPassDesc desc);

    bool InitRenderPass(ResourceManager& resource_manager);

    IRHIPipelineStateObject& GetPipelineStateObject();
    IRHIRootSignature& GetRootSignature();
    IRHIShaderTable* GetRayTracingShaderTable() const;
    const RendererInterface::RayTracingPassDesc* GetRayTracingPassDesc() const;

    RendererInterface::RenderPassType GetRenderPassType() const;
    RendererInterface::PrimitiveTopology GetPrimitiveTopology() const;

    const RootSignatureAllocation& GetRootSignatureAllocation(const std::string& name) const;
    const std::vector<RootSignatureAllocation>& GetRootSignatureAllocations(const std::string& name) const;
    const RootSignatureAllocation* FindRootSignatureAllocation(const std::string& name) const;
    bool HasRootSignatureAllocation(const std::string& name) const;
    DrawValidationResult ValidateDrawDesc(const RendererInterface::RenderPassDrawDesc& draw_desc) const;

    IRHIDescriptorUpdater& GetDescriptorUpdater();

    std::pair<int, int> GetViewportSize() const;
    void SetViewportSize(int width, int height);
    bool UpdateGraphicsRenderState(
        IRHIDevice& device,
        IRHISwapChain& swap_chain,
        const RendererInterface::RenderStateDesc& render_state,
        std::shared_ptr<IRHIResource>& out_retired_pipeline_state_object);
    bool IsInitialized() const;
    
protected:
    RendererInterface::RenderPassDesc m_desc;
    
    IRHIRootSignatureHelper m_root_signature_helper;
    
    std::shared_ptr<IRHIDescriptorUpdater> m_descriptor_updater;
    std::shared_ptr<IRHIPipelineStateObject> m_pipeline_state_object;

    std::map<std::string, std::vector<RootSignatureAllocation>> m_shader_parameter_mapping;

    int m_viewport_width{-1};
    int m_viewport_height{-1};
    bool m_init_success{false};
};
