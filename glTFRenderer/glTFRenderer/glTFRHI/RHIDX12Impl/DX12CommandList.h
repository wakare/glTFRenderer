#pragma once
#include "DX12Common.h"
#include "DX12CommandAllocator.h"
#include "../RHIInterface/IRHICommandList.h"

class DX12CommandList : public IRHICommandList
{
public:
    DX12CommandList();
    virtual ~DX12CommandList() override;
    
    virtual bool InitCommandList(IRHIDevice& device, IRHICommandAllocator& commandAllocator) override;

    ID3D12GraphicsCommandList* GetCommandList() const { return m_command_list.Get(); }
    ID3D12GraphicsCommandList4* GetDXRCommandList() const { return m_dxr_command_list.Get(); }
    
private:
    ComPtr<ID3D12GraphicsCommandList> m_command_list;
    ComPtr<ID3D12GraphicsCommandList4> m_dxr_command_list;
};
