#include "DX12CommandSignature.h"

#include "DX12Device.h"
#include "DX12RootSignature.h"
#include "DX12Utils.h"

DX12CommandSignature::DX12CommandSignature()
{
}

bool DX12CommandSignature::InitCommandSignature(IRHIDevice& device, IRHIRootSignature& root_signature)
{
    auto* dx_device = dynamic_cast<DX12Device&>(device).GetDevice();
    auto* dx_root_signature = dynamic_cast<DX12RootSignature&>(root_signature).GetRootSignature();
    
    std::vector<D3D12_INDIRECT_ARGUMENT_DESC> indirect_argument_descs;
    indirect_argument_descs.reserve(m_desc.m_indirect_arguments.size());

    bool need_bind_root_signature = false;
    for (const auto& argument : m_desc.m_indirect_arguments)
    {
        switch (argument.type) {
        case RHIIndirectArgType::Draw:
            break;
            
        case RHIIndirectArgType::DrawIndexed:
            break;
        case RHIIndirectArgType::Dispatch:
        case RHIIndirectArgType::DispatchRays:
        case RHIIndirectArgType::DispatchMesh:
            break;
            
        case RHIIndirectArgType::VertexBufferView:
            break;
            
        case RHIIndirectArgType::IndexBufferView:
            break;
            
        case RHIIndirectArgType::ConstantBufferView:
        case RHIIndirectArgType::ShaderResourceView:
        case RHIIndirectArgType::UnOrderAccessView:
            need_bind_root_signature = true;
            break;

        case RHIIndirectArgType::Constant:
            need_bind_root_signature = true;
            // TODO:
            break;
        }
        
        indirect_argument_descs.push_back(DX12ConverterUtils::ConvertToIndirectArgumentDesc(argument));
    }

    D3D12_COMMAND_SIGNATURE_DESC command_signature_desc = {};
    command_signature_desc.pArgumentDescs = indirect_argument_descs.data();
    command_signature_desc.NumArgumentDescs = indirect_argument_descs.size();
    command_signature_desc.ByteStride =  m_desc.stride;

    THROW_IF_FAILED(dx_device->CreateCommandSignature(&command_signature_desc, need_bind_root_signature ? dx_root_signature : nullptr, IID_PPV_ARGS(&m_command_signature)))
    
    return true;
}
