#pragma once
#include "glTFRenderInterfaceBase.h"
#include "glTFRHI/RHIUtils.h"
#include "glTFRHI/RHIInterface/IRHIRootSignatureHelper.h"

template<unsigned TableRangeCount>
class glTFRenderInterfaceSRVTable : public glTFRenderInterfaceWithRSAllocation
{
public:
    glTFRenderInterfaceSRVTable()
        : m_handle(0)
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
        if (m_handle)
        {
            RHIUtils::Instance().SetDTToRootParameterSlot(resource_manager.GetCommandListForRecord(), GetRSAllocation().parameter_index, m_handle, is_graphics_pipeline);    
        }
        
        return true;
    }

    virtual void ApplyShaderDefineImpl(RHIShaderPreDefineMacros& out_shader_pre_define_macros) const override
    {
        char register_index_name[16] = {'\0'};

        GLTF_CHECK(m_names.size() == TableRangeCount);

        unsigned register_offset = 0;
        for (const auto& name : m_names)
        {
            (void)snprintf(register_index_name, sizeof(register_index_name), "register(t%d)", GetRSAllocation().register_index + register_offset);
            out_shader_pre_define_macros.AddMacro(name, register_index_name);
            ++register_offset;
        }
    }

    void SetSRVRegisterNames(const std::vector<std::string>& names) {m_names = names;}
    void SetGPUHandle(RHIGPUDescriptorHandle gpu_handle) {m_handle = gpu_handle; }
    
protected:
    std::vector<std::string> m_names;
    RHIGPUDescriptorHandle m_handle;
};

// Bindless specific situation
class glTFRenderInterfaceSRVTableBindless : public glTFRenderInterfaceSRVTable<UINT_MAX>
{
    virtual void ApplyShaderDefineImpl(RHIShaderPreDefineMacros& out_shader_pre_define_macros) const override
    {
        GLTF_CHECK(m_names.size() == 1);
        
        char register_index_name[16] = {'\0'};

        (void)snprintf(register_index_name, sizeof(register_index_name), "register(t%d)", GetRSAllocation().register_index);
        out_shader_pre_define_macros.AddMacro(m_names.front(), register_index_name);
    }
};