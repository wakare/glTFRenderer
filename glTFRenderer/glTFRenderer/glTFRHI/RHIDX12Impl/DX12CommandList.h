#pragma once
#include "DX12Common.h"
#include "DX12CommandAllocator.h"
#include "glTFRHI/RHIInterface/IRHICommandList.h"

class IRHIFence;

class DX12CommandList : public IRHICommandList
{
public:
    DECLARE_NON_COPYABLE_AND_DEFAULT_CTOR_VDTOR(DX12CommandList)
    
    virtual bool InitCommandList(IRHIDevice& device, IRHICommandAllocator& commandAllocator) override;
    virtual bool WaitCommandList() override;
    virtual bool BeginRecordCommandList() override;
    virtual bool EndRecordCommandList() override;
    
    ID3D12GraphicsCommandList* GetCommandList() const { return m_command_list.Get(); }
    ID3D12GraphicsCommandList4* GetDXRCommandList() const { return m_dxr_command_list.Get(); }

    virtual bool Release(glTFRenderResourceManager&) override;
    
private:
    ComPtr<ID3D12GraphicsCommandList> m_command_list {nullptr};
    ComPtr<ID3D12GraphicsCommandList4> m_dxr_command_list {nullptr};
};
