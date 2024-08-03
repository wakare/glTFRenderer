#pragma once
#include "glTFRHI/RHIInterface/IRHIRootSignatureHelper.h"

class IRHIBufferDescriptorAllocation;
class IRHICommandList;
class IRHIDescriptorUpdater;
class glTFRenderResourceManager;

// glTF render pass interface can composite with render pass class, should be derived in class declaration
class glTFRenderInterfaceBase
{
public:
    virtual ~glTFRenderInterfaceBase() = default;
    
    bool InitInterface(glTFRenderResourceManager& resource_manager);
    bool ApplyInterface(glTFRenderResourceManager& resource_manager, RHIPipelineType pipeline_type, IRHIDescriptorUpdater& descriptor_updater);
    bool ApplyRootSignature(IRHIRootSignatureHelper& root_signature);
    void ApplyShaderDefine(RHIShaderPreDefineMacros& out_shader_pre_define_macros) const;
    
protected:
    virtual bool PreInitInterfaceImpl(glTFRenderResourceManager& resource_manager) {return true;}
    virtual bool InitInterfaceImpl(glTFRenderResourceManager& resource_manager) = 0;
    virtual bool PostInitInterfaceImpl(glTFRenderResourceManager& resource_manager) {return true;}
    
    virtual bool ApplyInterfaceImpl(IRHICommandList& command_list, RHIPipelineType pipeline_type, IRHIDescriptorUpdater& descriptor_updater) = 0;
    virtual bool ApplyRootSignatureImpl(IRHIRootSignatureHelper& root_signature) = 0;
    virtual void ApplyShaderDefineImpl(RHIShaderPreDefineMacros& out_shader_pre_define_macros) const = 0;
    
    void AddInterface(const std::shared_ptr<glTFRenderInterfaceBase>& render_interface);

    static std::shared_ptr<IRHIBufferDescriptorAllocation> CreateBufferDescriptor();
    
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

class glTFRenderInterfaceBaseWithDefaultImpl : public glTFRenderInterfaceBase
{
protected:
    virtual bool InitInterfaceImpl(glTFRenderResourceManager& resource_manager) override { return true; }
    virtual bool ApplyInterfaceImpl(IRHICommandList& command_list, RHIPipelineType pipeline_type, IRHIDescriptorUpdater& descriptor_updater) override {return true;}
    virtual bool ApplyRootSignatureImpl(IRHIRootSignatureHelper& root_signature) override { return true; }
    virtual void ApplyShaderDefineImpl(RHIShaderPreDefineMacros& out_shader_pre_define_macros) const override {}
    
};

class glTFRenderInterfaceWithRSAllocation : public glTFRenderInterfaceBase
{
public:
    glTFRenderInterfaceWithRSAllocation(const char* name)
        : m_name(name)
    {}
    
    const RootSignatureAllocation& GetRSAllocation() const {return m_allocation; }

    virtual void ApplyShaderDefineImpl(RHIShaderPreDefineMacros& out_shader_pre_define_macros) const override;
    
protected:
    std::string m_name;
    RootSignatureAllocation m_allocation;
};

class glTFRenderInterfaceCanUploadDataFromCPU
{
public:
    virtual ~glTFRenderInterfaceCanUploadDataFromCPU() = default;
    virtual bool UploadCPUBuffer(glTFRenderResourceManager& resource_manager, const void* data, size_t offset, size_t size) = 0;
};