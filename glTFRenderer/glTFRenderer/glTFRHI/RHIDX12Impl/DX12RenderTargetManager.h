#pragma once
#include <d3d12.h>
#include <memory>

#include "glTFRHI/RHIInterface/IRHIRenderTargetManager.h"

class DX12RenderTargetManager : public IRHIRenderTargetManager
{
public:
    DX12RenderTargetManager();
    virtual ~DX12RenderTargetManager() override;
    
    virtual bool InitRenderTargetManager(IRHIDevice& device, size_t max_render_target_count) override;
    virtual std::shared_ptr<IRHIRenderTarget> CreateRenderTarget(IRHIDevice& device, glTFRenderResourceManager& resource_manager, const
                                                                 RHITextureDesc& texture_desc, const RHIRenderTargetDesc& render_target_desc) override;
    virtual std::vector<std::shared_ptr<IRHIRenderTarget>> CreateRenderTargetFromSwapChain(IRHIDevice& device, glTFRenderResourceManager& resource_manager, IRHISwapChain& swap_chain, RHITextureClearValue clear_value) override;
    virtual bool ClearRenderTarget(IRHICommandList& command_list, const std::vector<IRHIRenderTarget*>& render_targets) override;
    virtual bool ClearRenderTarget(IRHICommandList& command_list, const std::vector<IRHIDescriptorAllocation*>& render_targets, const
                                   RHITextureClearValue& render_target_clear_value, const RHITextureClearValue& depth_stencil_clear_value) override;
    virtual bool BindRenderTarget(IRHICommandList& command_list, const std::vector<IRHIRenderTarget*>& render_targets, /*optional*/ IRHIRenderTarget* depth_stencil) override;
    virtual bool BindRenderTarget(IRHICommandList& command_list, const std::vector<IRHIDescriptorAllocation*>& render_targets, /*optional*/ IRHIDescriptorAllocation* depth_stencil) override;
    
private:
    std::shared_ptr<IRHIRenderTarget> CreateRenderTargetWithResource(IRHIDevice& device, glTFRenderResourceManager& resource_manager, RHIRenderTargetType type,
                                                                     RHIDataFormat descriptor_format, std::shared_ptr<IRHITextureAllocation> texture, const D3D12_CLEAR_VALUE& clear_value);
   
    std::vector<std::shared_ptr<IRHITextureAllocation>> m_external_textures;
};
