#include "glTFRenderPassBase.h"
#include "../glTFRHI/RHIResourceFactoryImpl.hpp"

glTFRenderPassBase::~glTFRenderPassBase()
= default;

bool glTFRenderPassBase::InitPass(glTFRenderResourceManager& resourceManager)
{    
    m_mainDescriptorHeap = RHIResourceFactory::CreateRHIResource<IRHIDescriptorHeap>();
    RETURN_IF_FALSE(SetupMainDescriptorHeap(resourceManager))

    m_rootSignature = RHIResourceFactory::CreateRHIResource<IRHIRootSignature>();
    RETURN_IF_FALSE(SetupRootSignature(resourceManager))
    
    m_pipelineStateObject = RHIResourceFactory::CreateRHIResource<IRHIPipelineStateObject>();
    RETURN_IF_FALSE(SetupPipelineStateObject(resourceManager))
    
    return true;
}

bool glTFRenderPassBase::RenderPass(glTFRenderResourceManager& resourceManager)
{
    RETURN_IF_FALSE(RHIUtils::Instance().SetDescriptorHeap(resourceManager.GetCommandList(), m_mainDescriptorHeap.get(), 1))
    RETURN_IF_FALSE(RHIUtils::Instance().SetRootSignature(resourceManager.GetCommandList(), *m_rootSignature))
    
    return true;
}

IRHIPipelineStateObject& glTFRenderPassBase::GetPSO() const
{
    return *m_pipelineStateObject;
}