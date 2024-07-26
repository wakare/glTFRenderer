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
    glTFRenderInterfaceSRVTable()
    {
    }

    virtual bool InitInterfaceImpl(glTFRenderResourceManager& resource_manager) override
    {
        return true;
    }
    
    virtual bool ApplyRootSignatureImpl(IRHIRootSignatureHelper& rootSignature) override
    {
        return rootSignature.AddTableRootParameter("TableParameter", RHIRootParameterDescriptorRangeType::SRV, TableRangeCount, TableRangeCount == UINT_MAX, m_allocation);
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

        descriptor_updater.BindDescriptor(command_list, pipeline_type, GetRSAllocation().parameter_index, *m_descriptor_table);
        
        return true;
    }
    
    virtual void ApplyShaderDefineImpl(RHIShaderPreDefineMacros& out_shader_pre_define_macros) const override
    {
        GLTF_CHECK(m_names.size() == TableRangeCount);
        
        unsigned register_offset = 0;
        for (const auto& name : m_names)
        {
            AddRootSignatureShaderRegisterDefine(out_shader_pre_define_macros, name, register_offset);
            ++register_offset;
        }
    }

    void SetSRVRegisterNames(const std::vector<std::string>& names) {m_names = names;}
    
    void AddTextureAllocations(const std::vector<std::shared_ptr<IRHITextureDescriptorAllocation>>& texture_allocations)
    {
        m_texture_descriptor_allocations.insert(m_texture_descriptor_allocations.end(), texture_allocations.begin(), texture_allocations.end());
    }
    
protected:
    std::vector<std::string> m_names;
    std::vector<std::shared_ptr<IRHITextureDescriptorAllocation>> m_texture_descriptor_allocations;
    std::shared_ptr<IRHIDescriptorTable> m_descriptor_table;
};

// Bindless specific situation
class glTFRenderInterfaceSRVTableBindless : public glTFRenderInterfaceSRVTable<UINT_MAX>
{
    virtual void ApplyShaderDefineImpl(RHIShaderPreDefineMacros& out_shader_pre_define_macros) const override
    {
        GLTF_CHECK(m_names.size() == 1);
        AddRootSignatureShaderRegisterDefine(out_shader_pre_define_macros, m_names[0]);
    }
};