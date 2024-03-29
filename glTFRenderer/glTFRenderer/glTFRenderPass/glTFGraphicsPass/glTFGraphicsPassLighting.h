#pragma once

#include "glTFGraphicsPassPostprocess.h"
#include "glTFLoader/glTFElementCommon.h"
#include "glTFRenderPass/glTFRenderPassCommon.h"
#include "glTFRenderPass/glTFRenderInterface/glTFRenderInterfaceSceneView.h"
#include "glTFRenderPass/glTFRenderInterface/glTFRenderInterfaceLighting.h"

class glTFGraphicsPassLighting : public glTFGraphicsPassPostprocess
{
public:
    glTFGraphicsPassLighting();

    virtual const char* PassName() override;
    virtual bool InitPass(glTFRenderResourceManager& resource_manager) override;
    virtual bool PreRenderPass(glTFRenderResourceManager& resource_manager) override;
    virtual bool PostRenderPass(glTFRenderResourceManager& resource_manager) override;
    
    virtual bool TryProcessSceneObject(glTFRenderResourceManager& resource_manager, const glTFSceneObjectBase& object) override;
    virtual bool FinishProcessSceneObject(glTFRenderResourceManager& resource_manager) override;
    
protected:
    virtual size_t GetMainDescriptorHeapSize() override;
    virtual bool SetupRootSignature(glTFRenderResourceManager& resource_manager) override;
    virtual bool SetupPipelineStateObject(glTFRenderResourceManager& resource_manager) override;
    
    std::shared_ptr<IRHIRenderTarget> m_base_pass_color_RT;
    std::shared_ptr<IRHIRenderTarget> m_normal_RT;
    
    RHIGPUDescriptorHandle m_base_pass_color_RT_SRV_Handle;
    RHIGPUDescriptorHandle m_depth_RT_SRV_Handle;
    RHIGPUDescriptorHandle m_normal_RT_SRV_Handle;

    RootSignatureAllocation m_base_color_and_depth_allocation;
};
