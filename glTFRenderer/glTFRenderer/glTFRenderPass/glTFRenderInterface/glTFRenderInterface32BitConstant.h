#pragma once
#include "glTFRenderInterfaceBase.h"
#include "glTFRenderPass/glTFRenderResourceManager.h"
#include "glTFRHI/RHIUtils.h"
#include <memory>

template<typename UploadStructType, unsigned count>
class glTFRenderInterface32BitConstant : public glTFRenderInterfaceWithRSAllocation, public glTFRenderInterfaceCanUploadDataFromCPU
{
public:
    glTFRenderInterface32BitConstant(const char* name)
        : glTFRenderInterfaceWithRSAllocation(name)
    {
        
    }
    
    virtual bool UploadCPUBuffer(glTFRenderResourceManager& resource_manager, const void* data, size_t offset, size_t size)
    {
        GLTF_CHECK(sizeof (m_data) == size);
        memcpy(m_data, static_cast<const char*>(data) + offset, size);
        return true;
    }
    
protected:
    virtual bool InitInterfaceImpl(glTFRenderResourceManager& resource_manager) override
    {
        static_assert(sizeof(UploadStructType) == 4);
        return true;
    }
    
    virtual bool ApplyInterfaceImpl(IRHICommandList& command_list, RHIPipelineType pipeline_type, IRHIDescriptorUpdater& descriptor_updater, unsigned
                                    frame_index) override
    {
        RETURN_IF_FALSE(RHIUtils::Instance().SetConstant32BitToRootParameterSlot(command_list,
           m_allocation.global_parameter_index, reinterpret_cast<unsigned*>(m_data), count, pipeline_type))
        
        return true;
    }
    
    virtual bool ApplyRootSignatureImpl(IRHIRootSignatureHelper& root_signature) override
    {
        return root_signature.AddConstantRootParameter(UploadStructType::Name, count, m_allocation);
    }
    
    UploadStructType m_data[count];
};
