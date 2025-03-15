#include "DX12DescriptorUpdater.h"

#include "DX12Utils.h"
#include "glTFRHI/RHIUtils.h"
#include "glTFRHI/RHIInterface/IRHIDescriptorManager.h"

bool DX12DescriptorUpdater::BindTextureDescriptorTable(IRHICommandList& command_list, RHIPipelineType pipeline, const RootSignatureAllocation& root_signature_allocation, const IRHIDescriptorAllocation& allocation)
{
    const auto& desc = allocation.GetDesc();
    switch (desc.m_dimension) {
    case RHIResourceDimension::UNKNOWN:
        GLTF_CHECK(false);
        break;
        
    case RHIResourceDimension::BUFFER:
        {
            if (desc.m_view_type == RHIViewType::RVT_CBV)
            {
                DX12Utils::DX12Instance().SetCBVToRootParameterSlot(command_list, root_signature_allocation.global_parameter_index, allocation, pipeline==RHIPipelineType::Graphics);    
            }
            else if (desc.m_view_type == RHIViewType::RVT_SRV)
            {
                DX12Utils::DX12Instance().SetSRVToRootParameterSlot(command_list, root_signature_allocation.global_parameter_index, allocation, pipeline==RHIPipelineType::Graphics);
            }
            else if (desc.m_view_type == RHIViewType::RVT_UAV)
            {
                DX12Utils::DX12Instance().SetDTToRootParameterSlot(command_list, root_signature_allocation.global_parameter_index, allocation, pipeline==RHIPipelineType::Graphics);
            }
            else
            {
                GLTF_CHECK(false);
            }
        }
        break;
    case RHIResourceDimension::TEXTURE1D:
    case RHIResourceDimension::TEXTURE1DARRAY:
    case RHIResourceDimension::TEXTURE2D:
    case RHIResourceDimension::TEXTURE2DARRAY:
    case RHIResourceDimension::TEXTURE2DMS:
    case RHIResourceDimension::TEXTURE2DMSARRAY:
    case RHIResourceDimension::TEXTURE3D:
    case RHIResourceDimension::TEXTURECUBE:
    case RHIResourceDimension::TEXTURECUBEARRAY:
        {
            DX12Utils::DX12Instance().SetDTToRootParameterSlot(command_list, root_signature_allocation.global_parameter_index, allocation, pipeline==RHIPipelineType::Graphics);
        }
        break;
    }
    
    return true;
}

bool DX12DescriptorUpdater::BindTextureDescriptorTable(IRHICommandList& command_list, RHIPipelineType pipeline, const RootSignatureAllocation& root_signature_allocation,
                                                       const IRHIDescriptorTable& allocation_table, RHIDescriptorRangeType descriptor_type)
{
    // TODO: More check
    DX12Utils::DX12Instance().SetDTToRootParameterSlot(command_list, root_signature_allocation.global_parameter_index, allocation_table, pipeline==RHIPipelineType::Graphics);
    return true;
}

bool DX12DescriptorUpdater::FinalizeUpdateDescriptors(IRHIDevice& device, IRHICommandList& command_list, IRHIRootSignature& root_signature)
{
    return true;
}
