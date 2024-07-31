#pragma once
#include "glTFRenderInterfaceBase.h"
#include "glTFRenderPass/glTFRenderResourceManager.h"
#include "glTFRHI/RHIResourceFactory.h"
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

    virtual bool InitInterfaceImpl(glTFRenderResourceManager& resource_manager) override
    {
        return true;
    }
    
    virtual bool ApplyRootSignatureImpl(IRHIRootSignatureHelper& rootSignature) override
    {
        return rootSignature.AddTableRootParameter(m_name, RHIRootParameterDescriptorRangeType::SRV, TableRangeCount, TableRangeCount == UINT_MAX, m_allocation);
    }

    virtual bool ApplyInterfaceImpl(IRHICommandList& command_list, RHIPipelineType pipeline_type, IRHIDescriptorUpdater& descriptor_updater) override
    {
        if (m_texture_descriptor_allocations.empty())
        {
            return true;
        }
        
        if (!m_descriptor_table)
        {
            m_descriptor_table = RHIResourceFactory::CreateRHIResource<IRHIDescriptorTable>();
            bool succeed = m_descriptor_table->Build(glTFRenderResourceManager::GetDevice(), m_texture_descriptor_allocations);
            GLTF_CHECK(succeed);
        }

        descriptor_updater.BindDescriptor(command_list, pipeline_type, GetRSAllocation().space, GetRSAllocation().global_parameter_index, *m_descriptor_table);
        
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