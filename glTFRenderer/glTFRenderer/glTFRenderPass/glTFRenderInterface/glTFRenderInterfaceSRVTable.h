#pragma once
#include "glTFRenderInterfaceBase.h"
#include "glTFRenderPass/glTFRenderResourceManager.h"
#include "glTFRHI/RHIUtils.h"
#include "glTFRHI/RHIInterface/IRHIDescriptorUpdater.h"
#include "glTFRHI/RHIInterface/IRHIRootSignatureHelper.h"

template<unsigned TableRangeCount>
class glTFRenderInterfaceSRVTable : public glTFRenderInterfaceWithRSAllocation
{
public:
    glTFRenderInterfaceSRVTable(const char* name)
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
        return root_signature.AddTableRootParameter(m_name, {RHIRootParameterDescriptorRangeType::SRV, TableRangeCount, false, TableRangeCount == UINT_MAX}, m_allocation);
    }

    virtual bool ApplyInterfaceImpl(IRHICommandList& command_list, RHIPipelineType pipeline_type, IRHIDescriptorUpdater& descriptor_updater, unsigned
                                    frame_index) override
    {
        if (m_descriptor_table)
        {
            // Important: Must do transition because DX may do transition when bind SRV table 
            for (const auto& entry : m_texture_descriptor_allocations)
            {
                entry->m_source->Transition(command_list, RHIResourceStateType::STATE_ALL_SHADER_RESOURCE);
            }
            descriptor_updater.BindDescriptor(command_list, pipeline_type, GetRSAllocation(), *m_descriptor_table);
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
class glTFRenderInterfaceSRVTableBindless : public glTFRenderInterfaceSRVTable<UINT_MAX>
{
public:
    glTFRenderInterfaceSRVTableBindless(const char* name)
        : glTFRenderInterfaceSRVTable<UINT_MAX>(name)
    {}
};