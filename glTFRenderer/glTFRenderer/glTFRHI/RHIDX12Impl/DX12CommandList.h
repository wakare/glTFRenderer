#pragma once
#include "DX12CommandAllocator.h"
#include "../RHIInterface/IRHICommandList.h"

class DX12CommandList : public IRHICommandList
{
public:
    DX12CommandList();
    virtual ~DX12CommandList() override;
    
    virtual bool InitCommandList(IRHIDevice& device, IRHICommandAllocator& commandAllocator) override;

    ID3D12GraphicsCommandList* GetCommandList() const {return m_command_list;}
private:
    ID3D12GraphicsCommandList* m_command_list;
};
