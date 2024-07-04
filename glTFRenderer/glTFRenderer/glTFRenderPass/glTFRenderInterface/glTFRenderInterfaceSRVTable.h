#pragma once
#include "glTFRenderInterfaceBase.h"
#include "glTFRenderPass/glTFRenderResourceManager.h"
#include "glTFRHI/RHIResourceFactory.h"
#include "glTFRHI/RHIUtils.h"
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

    virtual bool ApplyInterfaceImpl(glTFRenderResourceManager& resource_manager, bool is_graphics_pipeline) override
    {
        if (!m_descriptor_table)
        {
            m_descriptor_table = RHIResourceFactory::CreateRHIResource<IRHIDescriptorTable>();
            bool succeed = m_descriptor_table->Build(glTFRenderResourceManager::GetDevice(), m_descriptor_allocations);
            GLTF_CHECK(succeed);
        }

        RHIUtils::Instance().SetDTToRootParameterSlot(resource_manager.GetCommandListForRecord(), GetRSAllocation().parameter_index, *m_descriptor_table, is_graphics_pipeline);
        
        return true;
    }

    virtual void ApplyShaderDefineImpl(RHIShaderPreDefineMacros& out_shader_pre_define_macros) const override
    {
        char register_index_name[32] = {'\0'};

        GLTF_CHECK(m_names.size() == TableRangeCount);

        unsigned register_offset = 0;
        for (const auto& name : m_names)
        {
            (void)snprintf(register_index_name, sizeof(register_index_name), "register(t%d, space%u)", GetRSAllocation().register_index + register_offset, GetRSAllocation().space);
            out_shader_pre_define_macros.AddMacro(name, register_index_name);
            ++register_offset;
        }
    }

    void SetSRVRegisterNames(const std::vector<std::string>& names) {m_names = names;}
    void AddBufferAllocations(const std::vector<std::shared_ptr<IRHIDescriptorAllocation>>& buffer_allocations)
    {
        m_descriptor_allocations.insert(m_descriptor_allocations.end(), buffer_allocations.begin(), buffer_allocations.end());    
    }

    void AddTextureAllocations(const std::vector<std::shared_ptr<IRHIDescriptorAllocation>>& texture_allocations)
    {
        m_descriptor_allocations.insert(m_descriptor_allocations.end(), texture_allocations.begin(), texture_allocations.end());
    }
    //void SetGPUHandle(RHIGPUDescriptorHandle gpu_handle) {m_handle = gpu_handle; }
    
protected:
    std::vector<std::string> m_names;
    std::vector<std::shared_ptr<IRHIDescriptorAllocation>> m_descriptor_allocations;
    std::shared_ptr<IRHIDescriptorTable> m_descriptor_table;
};

// Bindless specific situation
class glTFRenderInterfaceSRVTableBindless : public glTFRenderInterfaceSRVTable<UINT_MAX>
{
    virtual void ApplyShaderDefineImpl(RHIShaderPreDefineMacros& out_shader_pre_define_macros) const override
    {
        GLTF_CHECK(m_names.size() == 1);
        
        char register_index_name[32] = {'\0'};

        (void)snprintf(register_index_name, sizeof(register_index_name), "register(t%d, space%u)", GetRSAllocation().register_index, GetRSAllocation().space);
        out_shader_pre_define_macros.AddMacro(m_names.front(), register_index_name);
    }
};