#pragma once
#include "glTFRenderPass/glTFRenderResourceManager.h"
#include "RHIInterface/IRHIRootSignatureHelper.h"

class IRHIDescriptorTable;
class IRHIBufferDescriptorAllocation;
class IRHICommandList;
class IRHIDescriptorUpdater;
class glTFRenderResourceManager;

// glTF render pass interface can composite with render pass class, should be derived in class declaration
class glTFRenderInterfaceBase
{
public:
    IMPL_NON_COPYABLE_AND_DEFAULT_CTOR_VDTOR(glTFRenderInterfaceBase)
    
    bool InitInterface(glTFRenderResourceManager& resource_manager);
    bool ApplyInterface(glTFRenderResourceManager& resource_manager, RHIPipelineType pipeline_type, IRHIDescriptorUpdater& descriptor_updater);
    bool ApplyRootSignature(IRHIRootSignatureHelper& root_signature);
    void ApplyShaderDefine(RHIShaderPreDefineMacros& out_shader_pre_define_macros);
    
protected:
    virtual bool PreInitInterfaceImpl(glTFRenderResourceManager& resource_manager);
    virtual bool InitInterfaceImpl(glTFRenderResourceManager& resource_manager);
    virtual bool PostInitInterfaceImpl(glTFRenderResourceManager& resource_manager);
    
    virtual bool ApplyInterfaceImpl(glTFRenderResourceManager& resource_manager, IRHICommandList& command_list, RHIPipelineType pipeline_type, IRHIDescriptorUpdater& descriptor_updater, unsigned
                                    frame_index);
    virtual bool ApplyRootSignatureImpl(IRHIRootSignatureHelper& root_signature);
    virtual void ApplyShaderDefineImpl(RHIShaderPreDefineMacros& out_shader_pre_define_macros);
    
    void AddInterface(const std::shared_ptr<glTFRenderInterfaceBase>& render_interface);

    static std::shared_ptr<IRHIDescriptorTable> CreateDescriptorTable();
    static void TraverseWithLambda(glTFRenderInterfaceBase& render_interface, const std::function<void(glTFRenderInterfaceBase&)>& lambda_function);
    
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
    virtual bool ApplyInterfaceImpl(glTFRenderResourceManager& resource_manager, IRHICommandList& command_list, RHIPipelineType pipeline_type, IRHIDescriptorUpdater& descriptor_updater, unsigned
                                    frame_index) override {return true;}
    virtual bool ApplyRootSignatureImpl(IRHIRootSignatureHelper& root_signature) override { return true; }
    virtual void ApplyShaderDefineImpl(RHIShaderPreDefineMacros& out_shader_pre_define_macros) override {}
    
};

class glTFRenderInterfaceWithRSAllocation : public glTFRenderInterfaceBase
{
public:
    glTFRenderInterfaceWithRSAllocation(const char* name)
        : m_name(name)
    {}
    
    const RootSignatureAllocation& GetRSAllocation() const {return m_allocation; }

    virtual void ApplyShaderDefineImpl(RHIShaderPreDefineMacros& out_shader_pre_define_macros) override;
    
protected:
    std::string m_name;
    RootSignatureAllocation m_allocation;
};

class glTFRenderInterfaceCPUAccessible
{
public:
    virtual ~glTFRenderInterfaceCPUAccessible() = default;
    virtual bool UploadBuffer(glTFRenderResourceManager& resource_manager, const void* data, size_t offset, size_t size) = 0;
    virtual bool DownloadBuffer(glTFRenderResourceManager& resource_manager, void* data, size_t size) {return false;}
};