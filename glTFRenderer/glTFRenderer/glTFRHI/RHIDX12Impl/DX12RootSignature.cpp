#include "DX12RootSignature.h"
#include "DX12Device.h"
#include <iostream>

DX12RootParameter::DX12RootParameter()
    : m_parameter({})
{
}

DX12RootParameter::~DX12RootParameter()
{
}

bool DX12RootParameter::InitAsConstant(unsigned constantValue, REGISTER_INDEX_TYPE registerIndex)
{
    SetType(RHIRootParameterType::Constant);
    m_parameter.ParameterType = D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS; 
    m_parameter.Constants.Num32BitValues = constantValue;
    m_parameter.Constants.ShaderRegister = registerIndex;
    m_parameter.Constants.RegisterSpace = 0;

    return true;
}

bool DX12RootParameter::InitAsCBV(REGISTER_INDEX_TYPE registerIndex)
{
    SetType(RHIRootParameterType::CBV);
    m_parameter.ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
    m_parameter.Descriptor.ShaderRegister = registerIndex;
    m_parameter.Descriptor.RegisterSpace = 0;

    return true;
}

bool DX12RootParameter::InitAsSRV(REGISTER_INDEX_TYPE registerIndex)
{
    SetType(RHIRootParameterType::SRV);
    m_parameter.ParameterType = D3D12_ROOT_PARAMETER_TYPE_SRV;
    m_parameter.Descriptor.ShaderRegister = registerIndex;
    m_parameter.Descriptor.RegisterSpace = 0;

    return true;
}

bool DX12RootParameter::InitAsUAV(REGISTER_INDEX_TYPE registerIndex)
{
    SetType(RHIRootParameterType::UAV);
    m_parameter.ParameterType = D3D12_ROOT_PARAMETER_TYPE_UAV;
    m_parameter.Descriptor.ShaderRegister = registerIndex;
    m_parameter.Descriptor.RegisterSpace = 0;

    return true;
}

bool DX12RootParameter::InitAsDescriptorTableRange(size_t rangeCount,
    const RHIRootParameterDescriptorRangeDesc* rangeDesc)
{
    if (rangeCount == 0 || rangeDesc == nullptr)
    {
        return false;
    }
    
    SetType(RHIRootParameterType::DescriptorTable);
    m_parameter.ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
    static auto _convertDX12RangeType = [](RHIRootParameterDescriptorRangeType type)
    {
        switch (type) {
            case RHIRootParameterDescriptorRangeType::CBV: return D3D12_DESCRIPTOR_RANGE_TYPE_CBV;
            case RHIRootParameterDescriptorRangeType::SRV: return D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
            case RHIRootParameterDescriptorRangeType::UAV: return D3D12_DESCRIPTOR_RANGE_TYPE_UAV;
        }

        // Unexpected situation
        assert(false);
        return (D3D12_DESCRIPTOR_RANGE_TYPE)RHICommonEnum::RHI_Unknown;
    };

    assert(m_ranges.empty());
    for (size_t i = 0; i < rangeCount; ++i)
    {
        const RHIRootParameterDescriptorRangeDesc& desc = rangeDesc[i]; 
        D3D12_DESCRIPTOR_RANGE range = {};
        range.NumDescriptors = desc.descriptorCount;
        range.RangeType = _convertDX12RangeType(desc.type);
        range.BaseShaderRegister = desc.baseRegisterIndex;

        // TODO: this parameter might be used for bindless mode
        range.RegisterSpace = 0;
        
        // TODO: use default value for most situation
        range.OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;
        
        m_ranges.push_back(range);
    }

    m_parameter.DescriptorTable = {};
    m_parameter.DescriptorTable.pDescriptorRanges = m_ranges.data();
    m_parameter.DescriptorTable.NumDescriptorRanges = m_ranges.size();

    return true;
}

DX12StaticSampler::DX12StaticSampler()
    : m_description({})
{
    m_description.Filter = D3D12_FILTER_ANISOTROPIC;
    m_description.AddressU = D3D12_TEXTURE_ADDRESS_MODE_MIRROR;
    m_description.AddressV = D3D12_TEXTURE_ADDRESS_MODE_MIRROR;
    m_description.AddressW = D3D12_TEXTURE_ADDRESS_MODE_MIRROR;
    m_description.MipLODBias = 0;
    m_description.MaxAnisotropy = 8;
    m_description.ComparisonFunc = D3D12_COMPARISON_FUNC_NEVER;
    m_description.BorderColor = D3D12_STATIC_BORDER_COLOR_OPAQUE_WHITE;
    m_description.MinLOD = 0.0f;
    m_description.MaxLOD = D3D12_FLOAT32_MAX;
    m_description.ShaderRegister = 0;
    m_description.RegisterSpace = 0;
    m_description.ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
}

bool DX12StaticSampler::InitStaticSampler(REGISTER_INDEX_TYPE registerIndex, RHIStaticSamplerAddressMode addressMode, RHIStaticSamplerFilterMode filterMode)
{
    m_description.ShaderRegister = registerIndex;
    
    switch (filterMode) {
        case RHIStaticSamplerFilterMode::Point:
            {
                m_description.Filter = D3D12_FILTER_MIN_MAG_MIP_POINT; 
            }
            break;
        case RHIStaticSamplerFilterMode::Linear:
            {
                m_description.Filter = D3D12_FILTER_MIN_MAG_MIP_LINEAR;
                m_description.MinLOD = 0;
                m_description.MaxLOD = D3D12_FLOAT32_MAX;
            }
            break;
        case RHIStaticSamplerFilterMode::Anisotropic:
            {
                m_description.Filter = D3D12_FILTER_ANISOTROPIC;
            }
            break;
        default:
            {
                assert(false);
                return false;
            }
    }

    switch (addressMode) {
        case RHIStaticSamplerAddressMode::Warp:
            {
                m_description.AddressU = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
                m_description.AddressV = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
                m_description.AddressW = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
            }
            break;
        case RHIStaticSamplerAddressMode::Mirror:
            {
                m_description.AddressU = D3D12_TEXTURE_ADDRESS_MODE_MIRROR;
                m_description.AddressV = D3D12_TEXTURE_ADDRESS_MODE_MIRROR;
                m_description.AddressW = D3D12_TEXTURE_ADDRESS_MODE_MIRROR;
            }
            break;
        case RHIStaticSamplerAddressMode::Clamp:
            {
                m_description.AddressU = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
                m_description.AddressV = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
                m_description.AddressW = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
            }
            break;
        case RHIStaticSamplerAddressMode::Border:
            {
                m_description.AddressU = D3D12_TEXTURE_ADDRESS_MODE_BORDER;
                m_description.AddressV = D3D12_TEXTURE_ADDRESS_MODE_BORDER;
                m_description.AddressW = D3D12_TEXTURE_ADDRESS_MODE_BORDER;
            }
            break;
        case RHIStaticSamplerAddressMode::MirrorOnce:
            {
                m_description.AddressU = D3D12_TEXTURE_ADDRESS_MODE_MIRROR_ONCE;
                m_description.AddressV = D3D12_TEXTURE_ADDRESS_MODE_MIRROR_ONCE;
                m_description.AddressW = D3D12_TEXTURE_ADDRESS_MODE_MIRROR_ONCE;
            }
            break;
        default:
            {
                assert(false);
                return false;
            }
    }

    return true;
}

DX12RootSignature::DX12RootSignature()
    : m_rootSignature(nullptr)
    , m_description({})
{
}

bool DX12RootSignature::InitRootSignature(IRHIDevice& device)
{
    if (!IsSpaceAllocated())
    {
        // Allocate space before init root signature!!
        assert(false);
        return false;
    }

    // Fill root parameters and static samplers
    std::vector<D3D12_ROOT_PARAMETER> dxRootParameters(m_rootParameters.size());
    for (size_t i = 0; i < dxRootParameters.size(); ++i)
    {
        const DX12RootParameter* dxRootParameter = static_cast<const DX12RootParameter*>(m_rootParameters[i].get());
        dxRootParameters[i] = dxRootParameter->GetParameter();
    }

    m_description.pParameters = dxRootParameters.data();
    m_description.NumParameters = m_rootParameters.size();
    
    std::vector<D3D12_STATIC_SAMPLER_DESC> dxStaticSamplers(m_staticSampler.size());
    for (size_t i = 0; i < m_staticSampler.size(); ++i)
    {
        const DX12StaticSampler* dxStaticSampler = static_cast<const DX12StaticSampler*>(m_staticSampler[i].get());
        dxStaticSamplers[i] = dxStaticSampler->GetStaticSamplerDesc();
    }

    m_description.pStaticSamplers = dxStaticSamplers.data();
    m_description.NumStaticSamplers = m_staticSampler.size();
    
    // TODO: This flag should be passed by application
    if (m_usage == RHIRootSignatureUsage::Default)
    {
        m_description.Flags |= D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;    
    }
    else if (m_usage == RHIRootSignatureUsage::LocalRS)
    {
        m_description.Flags |= D3D12_ROOT_SIGNATURE_FLAG_LOCAL_ROOT_SIGNATURE;    
    }
    
    ID3DBlob* signature = nullptr;
    ID3DBlob* error = nullptr;
    if (FAILED(D3D12SerializeRootSignature(&m_description, D3D_ROOT_SIGNATURE_VERSION_1, &signature, &error)))
    {
        std::cerr << "[ERROR] Serialize root signature failed with reason: " << static_cast<const char*>(error->GetBufferPointer()) << std::endl;
        return false;
    }
    
    auto* dxDevice = dynamic_cast<DX12Device&>(device).GetDevice();
    THROW_IF_FAILED(dxDevice->CreateRootSignature(0, signature->GetBufferPointer(), signature->GetBufferSize(), IID_PPV_ARGS(&m_rootSignature)))

    // TODO: Use ComPtr?
    if (signature)
    {
        signature->Release();
    }
    
    if (error)
    {
        error->Release();
    }
    
    return true;
}

ID3D12RootSignature* DX12RootSignature::GetRootSignature() const
{
    return m_rootSignature.Get();
}
