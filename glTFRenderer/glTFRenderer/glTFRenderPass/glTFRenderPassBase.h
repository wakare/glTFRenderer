#pragma once
#include "glTFRenderResourceManager.h"
#include "glTFApp/glTFPassOptionRenderFlags.h"
#include "glTFRenderInterface/glTFRenderInterfaceBase.h"
#include "glTFRHI/RHIInterface/IRHIRootSignatureHelper.h"
#include "glTFScene/glTFSceneObjectBase.h"

struct glTFSceneViewRenderFlags;
class glTFMaterialBase;
class IRHIRootSignature;
class IRHIPipelineStateObject;
class IRHIDescriptorHeap;
class IRHICommandList;

enum RenderPassCommonEnum
{
    MainDescriptorSize = 64,
};

enum class PipelineType
{
    Graphics,
    Compute,
    RayTracing,
};

class glTFRenderPassBase : public glTFUniqueObject<glTFRenderPassBase>
{
public:
    glTFRenderPassBase() = default;
    
    glTFRenderPassBase(const glTFRenderPassBase&) = delete;
    glTFRenderPassBase(glTFRenderPassBase&&) = delete;

    glTFRenderPassBase& operator=(const glTFRenderPassBase&) = delete;
    glTFRenderPassBase& operator=(glTFRenderPassBase&&) = delete;

    virtual ~glTFRenderPassBase() = 0;
    
    virtual const char* PassName() = 0;
    virtual bool InitPass(glTFRenderResourceManager& resource_manager);
    virtual bool PreRenderPass(glTFRenderResourceManager& resource_manager);
    virtual bool RenderPass(glTFRenderResourceManager& resource_manager);
    virtual bool PostRenderPass(glTFRenderResourceManager& resource_manager);

    virtual bool TryProcessSceneObject(glTFRenderResourceManager& resourceManager, const glTFSceneObjectBase& object) {return true; }
    virtual bool FinishProcessSceneObject(glTFRenderResourceManager& resourceManager) {return true; }
    virtual void UpdateRenderFlags(const glTFPassOptionRenderFlags& render_flags) {}

    virtual bool NeedRendering() const {return true; }
    virtual bool UpdateGUIWidgets();
    
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
        return nullptr;
    }
    
protected:
    // Must be implement in final render pass class
    virtual size_t GetMainDescriptorHeapSize() {return MainDescriptorSize;}
    virtual bool SetupRootSignature(glTFRenderResourceManager& resource_manager);
    virtual bool SetupPipelineStateObject(glTFRenderResourceManager& resource_manager) = 0;
    virtual PipelineType GetPipelineType() const = 0;
    
    void AddRenderInterface(const std::shared_ptr<glTFRenderInterfaceBase>& render_interface);

    IRHIRootSignatureHelper m_root_signature_helper;
    
    std::shared_ptr<IRHIPipelineStateObject> m_pipeline_state_object;
    
    // CBV_SRV_UAV Heaps, can only bind one in render pass
    std::shared_ptr<IRHIDescriptorHeap> m_main_descriptor_heap;

    std::vector<std::shared_ptr<glTFRenderInterfaceBase>> m_render_interfaces;
};
