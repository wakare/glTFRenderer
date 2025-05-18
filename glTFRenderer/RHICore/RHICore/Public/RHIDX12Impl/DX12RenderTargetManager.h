#pragma once
#include <d3d12.h>
#include <memory>

#include "IRHIRenderTargetManager.h"

class DX12RenderTargetManager : public IRHIRenderTargetManager
{
public:
    DX12RenderTargetManager();
    virtual ~DX12RenderTargetManager() override;
    
    virtual bool InitRenderTargetManager(IRHIDevice& device, size_t max_render_target_count) override;
    virtual std::shared_ptr<IRHITextureDescriptorAllocation> CreateRenderTarget(IRHIDevice& device, IRHIMemoryManager& memory_manager, const RHITextureDesc& texture_desc, const RHIRenderTargetDesc& render_target_desc) override;
    virtual std::vector<std::shared_ptr<IRHITextureDescriptorAllocation>> CreateRenderTargetFromSwapChain(IRHIDevice& device, IRHIMemoryManager& memory_manager, IRHISwapChain& swap_chain, RHITextureClearValue clear_value) override;
    virtual bool ClearRenderTarget(IRHICommandList& command_list, const std::vector<IRHIDescriptorAllocation*>& render_targets) override;
    virtual bool BindRenderTarget(IRHICommandList& command_list, const std::vector<IRHIDescriptorAllocation*>& render_targets) override;
    
private:
    std::shared_ptr<IRHITextureDescriptorAllocation> CreateRenderTargetWithResource(IRHIDevice& device, IRHIMemoryManager
        & memory_manager,
        RHIRenderTargetType type, RHIDataFormat descriptor_format, std::shared_ptr<IRHITextureAllocation> texture, const D3D12_CLEAR_VALUE& clear_value);
   
    std::vector<std::shared_ptr<IRHITextureAllocation>> m_external_textures;
};
