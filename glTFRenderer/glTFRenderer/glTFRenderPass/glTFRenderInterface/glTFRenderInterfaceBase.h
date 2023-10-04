#pragma once
#include "glTFRenderPass/glTFRenderResourceManager.h"
#include "glTFRHI/RHIInterface/IRHIRootSignatureHelper.h"

// glTF render pass interface can composite with render pass class, should be derived in class declaration
class glTFRenderInterfaceBase
{
public:
    virtual ~glTFRenderInterfaceBase() = default;
    
    virtual bool InitInterface(glTFRenderResourceManager& resource_manager) = 0;
    virtual bool ApplyInterface(glTFRenderResourceManager& resource_manager, bool isGraphicsPipeline) = 0;
    virtual bool ApplyRootSignature(IRHIRootSignatureHelper& root_signature) = 0;
    virtual void UpdateShaderCompileDefine(RHIShaderPreDefineMacros& out_shader_pre_define_macros) const = 0;
    
protected:
    void AddInterface(const std::shared_ptr<glTFRenderInterfaceBase>& render_interface);
    
    template<typename RenderInterface>
    const RenderInterface* GetRenderInterface() const
    {
        for (const auto& render_interface : m_sub_interfaces)
        {
            if (auto result = dynamic_cast<RenderInterface*>(render_interface.get()))
            {
                return result;
            }
        }
        return nullptr;
    }

    template<typename RenderInterface>
    RenderInterface* GetRenderInterface()
    {
        for (const auto& render_interface : m_sub_interfaces)
        {
            if (auto result = dynamic_cast<RenderInterface*>(render_interface.get()))
            {
                return result;
            }
        }
        return nullptr;
    }
        
    std::vector<std::shared_ptr<glTFRenderInterfaceBase>> m_sub_interfaces;
};

class glTFRenderInterfaceWithRSAllocation : public glTFRenderInterfaceBase
{
public:
    const RootSignatureAllocation& GetRSAllocation() const {return m_allocation; }
    
protected:
    RootSignatureAllocation m_allocation;
};

class glTFRenderInterfaceCanUploadDataFromCPU
{
public:
    virtual ~glTFRenderInterfaceCanUploadDataFromCPU() = default;
    
    virtual bool UpdateCPUBuffer(const void* data, size_t size) = 0;
};