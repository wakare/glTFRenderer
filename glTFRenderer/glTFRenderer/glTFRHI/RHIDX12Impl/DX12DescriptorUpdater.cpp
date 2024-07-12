#include "DX12DescriptorUpdater.h"

#include "DX12Utils.h"
#include "glTFRHI/RHIUtils.h"
#include "glTFRHI/RHIInterface/IRHIDescriptorManager.h"

bool DX12DescriptorUpdater::BindDescriptor(IRHICommandList& command_list, RHIPipelineType pipeline, unsigned slot, const IRHIDescriptorAllocation& allocation)
{
    switch (allocation.m_view_desc.dimension) {
    case RHIResourceDimension::UNKNOWN:
        GLTF_CHECK(false);
        break;
        
    case RHIResourceDimension::BUFFER:
        {
            if (allocation.m_view_desc.view_type == RHIViewType::RVT_CBV)
            {
                DX12Utils::DX12Instance().SetCBVToRootParameterSlot(command_list, slot, allocation, pipeline==RHIPipelineType::Graphics);    
            }
            else if (allocation.m_view_desc.view_type == RHIViewType::RVT_SRV)
            {
                DX12Utils::DX12Instance().SetSRVToRootParameterSlot(command_list, slot, allocation, pipeline==RHIPipelineType::Graphics);
            }
            else if (allocation.m_view_desc.view_type == RHIViewType::RVT_UAV)
            {
                DX12Utils::DX12Instance().SetDTToRootParameterSlot(command_list, slot, allocation, pipeline==RHIPipelineType::Graphics);
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
            DX12Utils::DX12Instance().SetDTToRootParameterSlot(command_list, slot, allocation, pipeline==RHIPipelineType::Graphics);
        }
        break;
    }
    
    return true;
}

bool DX12DescriptorUpdater::BindDescriptor(IRHICommandList& command_list, RHIPipelineType pipeline, unsigned slot,
    const IRHIDescriptorTable& allocation_table)
{
    // TODO: More check
    DX12Utils::DX12Instance().SetDTToRootParameterSlot(command_list, slot, allocation_table, pipeline==RHIPipelineType::Graphics);
    return true;
}
