#pragma once
#include "glTFRenderInterfaceBase.h"
#include "glTFRenderPass/glTFRenderResourceManager.h"
#include "glTFRHI/RHIUtils.h"
#include "glTFRHI/RHIInterface/IRHIDescriptorUpdater.h"
#include "glTFRHI/RHIInterface/IRHIRootSignatureHelper.h"

template<unsigned TableRangeCount, RHIRootParameterDescriptorRangeType TextureType>
class glTFRenderInterfaceTextureTable : public glTFRenderInterfaceWithRSAllocation
{
public:
    glTFRenderInterfaceTextureTable(const char* name)
        : glTFRenderInterfaceWithRSAllocation(name)
    {
    }

    virtual bool PostInitInterfaceImpl(glTFRenderResourceManager& resource_manager) override
    {
        if (m_texture_descriptor_allocations.empty())
        {
            return true;
        }
        
        if (!m_descriptor_table)
        {
            m_descriptor_table = CreateDescriptorTable();
            bool succeed = m_descriptor_table->Build(resource_manager.GetDevice(), m_texture_descriptor_allocations);
            GLTF_CHECK(succeed);
        }
        
        return true;
    }
    
    virtual bool ApplyRootSignatureImpl(IRHIRootSignatureHelper& root_signature) override
    {
        return root_signature.AddTableRootParameter(m_name, {TextureType, TableRangeCount, false, TableRangeCount == UINT_MAX}, m_allocation);
    }

    virtual bool ApplyInterfaceImpl(IRHICommandList& command_list, RHIPipelineType pipeline_type, IRHIDescriptorUpdater& descriptor_updater, unsigned
                                    frame_index) override
    {
        if (m_descriptor_table)
        {
            // Important: Must do transition because DX may do transition when bind SRV table 
            for (const auto& entry : m_texture_descriptor_allocations)
            {
                if (TextureType== RHIRootParameterDescriptorRangeType::SRV)
                {
                    entry->m_source->Transition(command_list, RHIResourceStateType::STATE_ALL_SHADER_RESOURCE);    
                }
                else if (TextureType == RHIRootParameterDescriptorRangeType::UAV)
                {
                    // Default behavior: clear uav content
                    RHIUtils::Instance().ClearUAVTexture(command_list, *entry);
                    entry->m_source->Transition(command_list, RHIResourceStateType::STATE_UNORDERED_ACCESS);
                }
            }
            descriptor_updater.BindTextureDescriptorTable(command_list, pipeline_type, GetRSAllocation(), *m_descriptor_table, TextureType);
        }
        
        return true;
    }
    
    void AddTextureAllocations(const std::vector<std::shared_ptr<IRHITextureDescriptorAllocation>>& texture_allocations)
    {
        m_texture_descriptor_allocations.insert(m_texture_descriptor_allocations.end(), texture_allocations.begin(), texture_allocations.end());
    }
    
protected:
    std::vector<std::shared_ptr<IRHITextureDescriptorAllocation>> m_texture_descriptor_allocations;
    std::shared_ptr<IRHIDescriptorTable> m_descriptor_table;
};

// Bindless specific situation
template<RHIRootParameterDescriptorRangeType TextureType>
class glTFRenderInterfaceTextureTableBindless : public glTFRenderInterfaceTextureTable<UINT_MAX, TextureType>
{
public:
    glTFRenderInterfaceTextureTableBindless(const char* name)
        : glTFRenderInterfaceTextureTable<UINT_MAX, TextureType>(name)
    {}
};