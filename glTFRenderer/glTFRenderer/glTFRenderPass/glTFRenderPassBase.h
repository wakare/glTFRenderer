#pragma once
#include "glTFRenderPassCommon.h"
#include "RenderGraphNodeUtil.h"
#include "glTFApp/glTFPassOptionRenderFlags.h"
#include "glTFRenderInterface/glTFRenderInterfaceBase.h"
#include "RHIInterface/IRHIRootSignatureHelper.h"
#include "glTFScene/glTFSceneObjectBase.h"

class IRHIDescriptorAllocation;
struct glTFSceneViewRenderFlags;
class RHIVertexStreamingManager;
class IRHIDescriptorUpdater;
class IRHIRenderPass;
class glTFMaterialBase;
class IRHIRootSignature;
class IRHIPipelineStateObject;
class IRHICommandList;

class glTFRenderPassBase : public glTFUniqueObject<glTFRenderPassBase>, public RenderGraphNodeUtil::RenderGraphNode
{
public:
    IMPL_NON_COPYABLE_AND_DEFAULT_CTOR_VDTOR(glTFRenderPassBase)

    virtual const char* PassName() = 0;
    
    virtual bool InitRenderInterface(glTFRenderResourceManager& resource_manager);
    virtual bool InitPass(glTFRenderResourceManager& resource_manager);
    virtual bool PreRenderPass(glTFRenderResourceManager& resource_manager);
    virtual bool RenderPass(glTFRenderResourceManager& resource_manager);
    virtual bool PostRenderPass(glTFRenderResourceManager& resource_manager);

    virtual bool TryProcessSceneObject(glTFRenderResourceManager& resource_manager, const glTFSceneObjectBase& object) { return true; }
    virtual bool FinishProcessSceneObject(glTFRenderResourceManager& resource_manager) { return true; }
    virtual void UpdateRenderFlags(const glTFPassOptionRenderFlags& render_flags) {}

    virtual bool IsRenderingEnabled() const { return m_rendering_enabled; }
    virtual void SetRenderingEnabled(bool enabled) { m_rendering_enabled = enabled;}
    virtual bool UpdateGUIWidgets();

    virtual bool ModifyFinalOutput(RenderGraphNodeUtil::RenderGraphNodeFinalOutput& final_output) override;
    
    template<typename RenderInterface>
    RenderInterface* GetRenderInterface()
    {
        for (const auto& render_interface : m_render_interfaces)
        {
            if (auto result = dynamic_cast<RenderInterface*>(render_interface.get()))
            {
                return result;
            }
        }
        //GLTF_CHECK(false);
        return nullptr;
    }

    template<typename RenderInterface>
    const RenderInterface* GetRenderInterface() const
    {
        for (const auto& render_interface : m_render_interfaces)
        {
            if (auto result = dynamic_cast<RenderInterface*>(render_interface.get()))
            {
                return result;
            }
        }
        GLTF_CHECK(false);
        return nullptr;
    }
    
protected:
    // Must be implement in final render pass class
    virtual bool SetupRootSignature(glTFRenderResourceManager& resource_manager);
    virtual bool SetupPipelineStateObject(glTFRenderResourceManager& resource_manager) = 0;
    virtual RHIPipelineType GetPipelineType() const = 0;
    
    void AddRenderInterface(const std::shared_ptr<glTFRenderInterfaceBase>& render_interface);
    bool BindDescriptor(IRHICommandList& command_list, const RootSignatureAllocation& root_signature_allocation, const IRHIDescriptorAllocation& allocation) const;
    
    virtual const RHIVertexStreamingManager& GetVertexStreamingManager(glTFRenderResourceManager& resource_manager) const;

    bool m_rendering_enabled = true;
    
    IRHIRootSignatureHelper m_root_signature_helper;
    
    std::shared_ptr<IRHIDescriptorUpdater> m_descriptor_updater;
    std::shared_ptr<IRHIPipelineStateObject> m_pipeline_state_object;
    std::vector<std::shared_ptr<glTFRenderInterfaceBase>> m_render_interfaces;
};
